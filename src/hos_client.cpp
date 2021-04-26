/*************************************************************************
    > File Name: hos_client_api.cpp
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:00:23 PM CST
 ************************************************************************/
extern "C"
{
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
}
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/threading/Executor.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include "hos_client.h"
#include "hos_hash.h"
#include "field_stat2.h"
#include "MESA_handle_logger.h"
#include "MESA_prof_load.h"

#if(__GNUC__ * 100 + __GNUC_MINOR__ * 10 + __GNUC_PATCHLEVEL__ >= 410)
#define atomic_add(x,y) __sync_add_and_fetch((x),(y))
#define atomic_read(x) __sync_add_and_fetch((x),0)
#define atomic_sub(x,y) __sync_sub_and_fetch((x),(y))
#else 
#define atomic_add(x,y) ((*(x))+=(y))
#define atomic_read(x) 	(*(x))
#define atomic_sub(x,y) ((*(x))-=(y))
#endif 

#define MAX_HOS_STRING_LEN 1024
#define HOS_ERROR_MESSAGE_SIZE (MAX_HOS_STRING_LEN - 1)
#define MAX_HOS_CLIENT_FD_NUM 65535
#define HOS_LOG_PATH "./tsglog/hoslog"

typedef struct data_info_s
{
    size_t *tx_pkts;
    size_t *tx_bytes;
    size_t *rx_pkts;
    size_t *rx_bytes;
    size_t *tx_failed_pkts;
    size_t *tx_failed_bytes;
    size_t *cache;
}data_info_t;

typedef struct fs2_info_s
{
    screen_stat_handle_t fs2_handle;
    int *line_ids;
    int *column_ids;
    void *reserved; //预留给每个fs2 handle用来存储自定义的数据
}fs2_info_t;

enum
{
    FS2_DATA_FLOW_STATE = 0,
    FS2_POOL_THREAD_STATE,
    FS2_RECORD_EVENTS,
};

typedef struct hos_config_s
{
    char ip[INET6_ADDRSTRLEN];
    char fs2_ip[INET6_ADDRSTRLEN];
    char accesskeyid[MAX_HOS_STRING_LEN];
    char secretkey[MAX_HOS_STRING_LEN];
    char log_path[MAX_HOS_STRING_LEN];
    char fs2_path[MAX_HOS_STRING_LEN];

    uint32_t port;
    uint32_t fs2_port;
    uint32_t fs2_fmt;
    uint32_t log_level;
    uint32_t pool_thread_size;
    uint32_t thread_num;
    uint32_t cache_size;
    uint32_t cache_count;
    uint32_t timeout;
}hos_config_t;

typedef struct hos_func_thread_s
{
    /* fd 管理线程 */
    pthread_t fd_thread;
    int fd_thread_status;
    /* fs2 管理线程 */
    fs2_info_t fs2_info[FS2_RECORD_EVENTS]; //0: data info; 1: fd info; 2 cache info; 3 PoolThread state
    pthread_t fs2_thread;
    int fs2_status;
#define HOS_FS2_START 1
#define HOS_FS2_STOP  2
}hos_func_thread_t;

typedef struct hos_client_handle_s
{
    Aws::S3::S3Client *S3Client;
    Aws::Vector<Aws::S3::Model::Bucket> buckets;
    std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor> executor;
    size_t count;  /* 记录了有多少个对象在使用hos */
    hos_config_t hos_config;
    hos_func_thread_t hos_func;
    void *log;
}hos_client_handle_t;

static struct hos_instance_s g_hos_instance;
static hos_client_handle_t g_hos_handle;//一个进程只允许有一个g_hos_handle
static std::mutex m_client_lock;
static hos_fd_context_t **g_fd_context;
static size_t (*g_fd_info)[MAX_HOS_CLIENT_FD_NUM + 1]; //fd 实际从3开始， fd[thread_id][0]记录register的fd，fd[thread_id][1]记录inject的fd
static Aws::SDKOptions g_options;

static inline size_t get_current_ms()
{
    struct timespec timenow;
    clock_gettime(CLOCK_MONOTONIC, &timenow);
    return (timenow.tv_sec * 1000 + timenow.tv_nsec / 1000 / 1000 );
}   

static size_t hash_get_min_free_fd(size_t thread_id)
{
    size_t i = 0;
    for (i = 3; i < MAX_HOS_CLIENT_FD_NUM + 1; i++)
    {
        if (!g_fd_info[thread_id][i])
        {
            g_fd_info[thread_id][i] = 1;
            g_fd_info[thread_id][HOS_FD_REGISTER]++;
            g_fd_info[thread_id][HOS_FD_FREE]--;

            return i; 
        }
    }
    return 0;
}

static int hos_delete_fd(size_t fd, size_t thread_id)
{
    if (fd == 0)
    {
        return HOS_PARAMETER_ERROR;
    }
    delete_context_by_fd(&g_fd_context[thread_id], fd);
    g_fd_info[thread_id][fd] = 0;
    g_fd_info[thread_id][HOS_FD_FREE]++;
    g_fd_info[thread_id][HOS_FD_INJECT]--;
    
    return HOS_CLIENT_OK;
}

static void PutObjectAsyncFinished(const Aws::S3::S3Client* S3Client, 
    const Aws::S3::Model::PutObjectRequest& request, 
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    const char *error = NULL;
    hos_fd_context_t *a_fd_context = NULL;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    const Aws::String& uuid = context->GetUUID();
    size_t thread_id, fd, stream_len;
    sscanf(uuid.c_str(), "%lu %lu %lu", &thread_id, &fd, &stream_len);
    if (g_fd_info[thread_id][fd])
    {
        a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    }
    if (a_fd_context == NULL)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
           "Not find the info of [thread_id:%d fd:%d]", thread_id, fd);

        if (hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle && hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved;
            data_info->tx_failed_pkts[thread_id]++;
            data_info->tx_failed_bytes[thread_id] += stream_len;
        }
        return ; 
    }

    bool result = outcome.IsSuccess();
    if (!result)
    {
        error = outcome.GetError().GetMessage().c_str();
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
            "[%s:%s] upload failed. error:%s",a_fd_context->bucket, a_fd_context->object, error);

        if (hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle && hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved;
            data_info->tx_failed_pkts[thread_id]++;
            data_info->tx_failed_bytes[thread_id] += stream_len;
        }
    }
    else
    {
        if (hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle && hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved;
            data_info->tx_pkts[thread_id]++;
            data_info->tx_bytes[thread_id] += stream_len;
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "[%s:%s] upload success. tx_pkts:%d, tx_bytes:%d",
                                    a_fd_context->bucket, a_fd_context->object,
                                    data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
        }
        else
        {
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "[%s:%s] upload success. stream size:%d", a_fd_context->bucket, a_fd_context->object, stream_len);
        }
    }
    put_finished_callback callback = (put_finished_callback)a_fd_context->callback;
    callback(result, a_fd_context->bucket, a_fd_context->object, error, a_fd_context->userdata);
    if (a_fd_context->mode & APPEND_MODE)
    {
        //APPEND MODE 保留fd
        atomic_add(&(a_fd_context->recive_cnt), 1);
    }
    else
    {
        //完整上传 删除fd
        hos_close_fd(fd, thread_id);
    }
}

static void hos_client_create()
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    void *log = g_hos_handle.log;

    if (g_hos_handle.S3Client != NULL)
    {
        g_hos_handle.count++;
        g_hos_instance.result = true;
        return ;
    }

    Aws::InitAPI(g_options);
    Aws::Client::ClientConfiguration config;
    Aws::Auth::AWSCredentials credentials(hos_conf->accesskeyid, hos_conf->secretkey);

    //初始化
    char endpoint[128];
    snprintf(endpoint, 128, "http://%s:%d/hos/", hos_conf->ip, hos_conf->port);
    config.endpointOverride = endpoint;
    config.verifySSL = false;
    config.enableEndpointDiscovery = true;
    if (hos_conf->pool_thread_size > 0)
    {
        //异步模式
        config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(hos_conf->pool_thread_size, Aws::Utils::Threading::OverflowPolicy::REJECT_IMMEDIATELY)); //支持线程池
    }
    else
    {
        //同步模式
    }

    g_hos_handle.S3Client = new Aws::S3::S3Client(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    /* 获取当前用户的所有的buckets */
    Aws::S3::Model::ListBucketsOutcome outcome = g_hos_handle.S3Client->ListBuckets();

    if (!outcome.IsSuccess())
    {
        delete g_hos_handle.S3Client;
        g_hos_handle.S3Client = NULL;
        Aws::ShutdownAPI(g_options);
        g_hos_instance.error_code = (size_t)outcome.GetError().GetErrorType() + 1;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, outcome.GetError().GetMessage().c_str());
        g_hos_instance.result = false;
        MESA_handle_runtime_log(log, RLOG_LV_FATAL, "hos_client_create", g_hos_instance.error_message);
        return;
    }

    g_hos_handle.buckets = outcome.GetResult().GetBuckets();
    g_hos_handle.count++;
    g_hos_handle.executor = std::dynamic_pointer_cast<Aws::Utils::Threading::PooledThreadExecutor>(config.executor);

    g_fd_context = (hos_fd_context_t **)calloc(hos_conf->thread_num, sizeof(hos_fd_context_t *));
    g_fd_info = (size_t (*)[MAX_HOS_CLIENT_FD_NUM + 1])calloc(hos_conf->thread_num, sizeof(size_t [MAX_HOS_CLIENT_FD_NUM + 1]));

    for (size_t i = 0; i < hos_conf->thread_num; i++)
    {
        g_fd_info[i][0] = 65533;
    }

    MESA_handle_runtime_log(log, RLOG_LV_DEBUG, "hos_client_create", "hos s3client create success, url:%s.",endpoint);
    g_hos_instance.result = true;
}

static bool hos_verify_bucket(const char *bucket)
{
    Aws::S3::Model::ListBucketsOutcome outcome = g_hos_handle.S3Client->ListBuckets();

    if (outcome.IsSuccess())
    {
        g_hos_handle.buckets = outcome.GetResult().GetBuckets();

        for (Aws::S3::Model::Bucket& new_bucket : g_hos_handle.buckets)
        {
            if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
            {
                MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, "hos_verify_bucket","bucket:%s exits", bucket);
                return true;
            }
        }
    }
    else
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_verify_bucket","error:%s", outcome.GetError().GetMessage().c_str());
    }
    return false;
}

static void *fs2_statistics(void *ptr)
{
    size_t i = 0;
    size_t rx_pkts_sum = 0;
    size_t rx_bytes_sum = 0;
    size_t tx_pkts_sum = 0;
    size_t tx_bytes_sum = 0;
    size_t tx_failed_bytes_sum = 0;
    size_t tx_failed_pkts_sum = 0;
    size_t cache_sum = 0;
    size_t rx_pkts_interval = 0;
    size_t rx_bytes_interval = 0;
    size_t tx_pkts_interval = 0;
    size_t tx_bytes_interval = 0;
    size_t tx_failed_bytes_interval = 0;
    size_t tx_failed_pkts_interval = 0;
    size_t cache_interval = 0;
    size_t rx_pkts_last = 0;
    size_t rx_bytes_last = 0;
    size_t tx_pkts_last = 0;
    size_t tx_bytes_last = 0;
    size_t tx_failed_bytes_last = 0;
    size_t tx_failed_pkts_last = 0;
    size_t cache_last = 0;
    fs2_info_t *fs2_info = NULL; 
    int PoolThread_state[3] = {0, 0, 0};//{PoolSize, Busy, TopBusy}
    int *busy = &PoolThread_state[1];
    int *top_busy = &PoolThread_state[2];
    int pool_history_sum = 0;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;

    PoolThread_state[0] = hos_conf->pool_thread_size;
    while(1)
    {
        if (hos_func->fs2_status == HOS_FS2_STOP)
        {
            break;
        }

        //pkts and bytes info
        rx_pkts_sum = 0;
        rx_bytes_sum = 0;
        tx_pkts_sum = 0;
        tx_bytes_sum = 0;
        tx_failed_bytes_sum = 0;
        tx_failed_pkts_sum = 0;
        cache_sum = 0;

        fs2_info = &hos_func->fs2_info[0];
        data_info_t *data_info = (data_info_t *)fs2_info->reserved;
        for (i = 0; i < hos_conf->thread_num; i++)
        {
            rx_pkts_sum += data_info->rx_pkts[i];
            rx_bytes_sum += data_info->rx_bytes[i];
            tx_pkts_sum += data_info->tx_pkts[i];
            tx_bytes_sum += data_info->tx_bytes[i];
            tx_failed_bytes_sum += data_info->tx_failed_bytes[i];
            tx_failed_pkts_sum += data_info->tx_failed_pkts[i];
            cache_sum += data_info->cache[i];
        }

        rx_pkts_interval = rx_pkts_sum - rx_pkts_last;
        rx_bytes_interval = rx_bytes_sum - rx_bytes_last;
        tx_pkts_interval = tx_pkts_sum - tx_pkts_last;
        tx_bytes_interval = tx_bytes_sum - tx_bytes_last;
        tx_failed_pkts_interval = tx_failed_pkts_sum - tx_failed_pkts_last;
        tx_failed_bytes_interval = tx_failed_bytes_sum - tx_failed_bytes_last;
        cache_interval = cache_sum - cache_last;

        rx_pkts_last = rx_pkts_sum;
        rx_bytes_last = rx_bytes_sum;
        tx_pkts_last = tx_pkts_sum;
        tx_bytes_last = tx_bytes_sum;
        tx_failed_bytes_last = tx_failed_bytes_sum;
        tx_failed_pkts_last = tx_failed_pkts_sum;
        cache_last = cache_sum;

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[5], FS_OP_SET, tx_failed_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[4], FS_OP_SET, tx_failed_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[6], FS_OP_SET, cache_interval);

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[4], FS_OP_SET, tx_failed_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[5], FS_OP_SET, tx_failed_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[6], FS_OP_SET, cache_sum);

        //PoolThread State
        if (hos_conf->pool_thread_size > 0)
        {
            *busy = g_hos_handle.executor->GetTaskSize();
            *top_busy = (*busy) > (*top_busy) ? (*busy) : (*top_busy);
            pool_history_sum += *busy;

            fs2_info = &hos_func->fs2_info[FS2_POOL_THREAD_STATE];
            for (i = 0; i < 3; i++)
            {
                FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[i], FS_OP_SET, PoolThread_state[i]);
            }
        }

        sleep(1);
    }
    pthread_exit(NULL);
}

static screen_stat_handle_t hos_init_fs2(char *app_name, int app_name_size)
{
    int value = 0;
    screen_stat_handle_t fs2_handle = FS_create_handle();
    hos_config_t *hos_conf = &g_hos_handle.hos_config;

    FS_set_para(fs2_handle, APP_NAME, app_name, app_name_size + 1);
    value = 1; //true
    FS_set_para(fs2_handle, FLUSH_BY_DATE, &value, sizeof(value));
    if (hos_conf->fs2_path != NULL)
    {
        FS_set_para(fs2_handle, OUTPUT_DEVICE, hos_conf->fs2_path, strlen(hos_conf->fs2_path) + 1);
    }
    value = 2;
    FS_set_para(fs2_handle, PRINT_MODE, &value, sizeof(value));
    value = 1;
    FS_set_para(fs2_handle, CREATE_THREAD, &value, sizeof(value));
    FS_set_para(fs2_handle, METRIS_FORMAT, &hos_conf->fs2_fmt, sizeof(hos_conf->fs2_fmt));
    FS_set_para(fs2_handle, STAT_CYCLE, &value, sizeof(value));
    value = 4096;
    FS_set_para(fs2_handle, MAX_STAT_FIELD_NUM, &value, sizeof(value));
    if (hos_conf->fs2_ip == NULL)
    {
        FS_set_para(fs2_handle, STATS_SERVER_IP, "127.0.0.1", strlen("127.0.0.1"));
    }
    else
    {
        FS_set_para(fs2_handle, STATS_SERVER_IP, hos_conf->fs2_ip, strlen(hos_conf->fs2_ip));
    }

    FS_set_para(fs2_handle, STATS_SERVER_PORT, &hos_conf->fs2_port, sizeof(hos_conf->fs2_port));

    value = FS_OUTPUT_STATSD;
    FS_set_para(fs2_handle, STATS_FORMAT, &value, sizeof(value));

    return fs2_handle;
}

static void hos_expand_fs2()
{
    fs2_info_t *fs2_info = NULL;
    screen_stat_handle_t fs2_handle = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    size_t i = 0;

    if (hos_func->fs2_info[0].fs2_handle)
        return;
    //data info
    /**********************************************************************************************************
     *              rx_pkts     rx_bytes    tx_pkts     tx_bytes    tx_failed_p     tx_failed_b     cache_bytes
     * current      10          100         1           100         0               0               100
     * total        100         1000        10          1000        0               0               100(无实意)
    ***********************************************************************************************************/
    fs2_info = &hos_func->fs2_info[FS2_DATA_FLOW_STATE];
    hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle = hos_init_fs2((char *)"hos-data", strlen("hos-data"));
    fs2_handle = hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle;
    fs2_info->line_ids = (int *)calloc(2, sizeof(int));
    fs2_info->column_ids = (int *)calloc(7, sizeof(int));

    const char *data_col[] = {"rx_pkts", "rx_bytes", "tx_pkts", "tx_bytes", "tx_failed_p", "tx_failed_b", "cache_bytes"};
    for (i = 0; i < sizeof(data_col) / sizeof(const char *); i++)
    {
        fs2_info->column_ids[i] = FS_register(fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, data_col[i]);
    }
    fs2_info->line_ids[0] = FS_register(fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, "current");
    fs2_info->line_ids[1] = FS_register(fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, "total");

    hos_func->fs2_status = HOS_FS2_START;
    data_info_t *data_info = (data_info_t *)calloc(1, sizeof(data_info_t));
    fs2_info->reserved = (void *)data_info;
    data_info->tx_pkts = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->tx_bytes = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->rx_pkts = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->rx_bytes = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->tx_failed_pkts = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->tx_failed_bytes = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    data_info->cache = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    FS_start(fs2_handle);

    if (hos_conf->pool_thread_size > 0)
    {
        //PoolThread state
        /*******************************************************
        *              PoolSize    Busy    TopBusy     AveBusy
        * ThreadNum    1000        500     800         650
        ********************************************************/
        fs2_info = &hos_func->fs2_info[FS2_POOL_THREAD_STATE];
        hos_func->fs2_info[FS2_POOL_THREAD_STATE].fs2_handle = hos_init_fs2((char *)"hos-poolthread", strlen("hos-poolthread"));
        fs2_handle = hos_func->fs2_info[FS2_POOL_THREAD_STATE].fs2_handle;
        fs2_info->line_ids = (int *)calloc(1, sizeof(int));
        fs2_info->column_ids = (int *)calloc(3, sizeof(int));

        const char *poolthread_col[3] = {"PoolSize", "Busy", "TopBusy"};
        for (i = 0; i < sizeof(poolthread_col) / sizeof(const char *); i++)
        {
            fs2_info->column_ids[i] = FS_register(fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, poolthread_col[i]);
        }
        fs2_info->line_ids[0] = FS_register(fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, "ThreadNum");

        FS_start(fs2_handle);
    }

    pthread_create(&hos_func->fs2_thread, NULL, fs2_statistics, NULL);

    return ;
}

static bool hos_putobject_async(Aws::S3::Model::PutObjectRequest& request, size_t stream_len,
    size_t thread_id, size_t fd, const char *bucket, const char *object)
{
    char buf[128];
    int ret = 0;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu %lu", thread_id, fd, stream_len);
    context->SetUUID(buf);

    Aws::S3::S3Client& S3Client = *(g_hos_handle.S3Client);
    ret = S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    if (ret)
    {
        //不算真正成功，需要等到PutObjectAsyncFinished的结果
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "PutObjectAsync success. [%s:%s]", bucket, object);
    }
    else
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "PutObjectAsync failed. [%s:%s]", bucket, object);

        if (hos_func->fs2_info[0].fs2_handle)
        {
            if (hos_func->fs2_info[0].reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info[0].reserved;
                data_info->tx_failed_pkts[thread_id]++;
                data_info->tx_failed_bytes[thread_id] += stream_len;
            }
        }
    }

    return ret;
}

static bool hos_putobject_sync(Aws::S3::Model::PutObjectRequest& request, size_t stream_len, size_t thread_id, size_t fd,
    const char *bucket, const char *object)
{
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;

    Aws::S3::S3Client& S3Client = *(g_hos_handle.S3Client);
    Aws::S3::Model::PutObjectOutcome Outcome = S3Client.PutObject(request);
    if (Outcome.IsSuccess())
    {
        if (hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle && hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved;
            data_info->tx_pkts[thread_id]++;
            data_info->tx_bytes[thread_id] += stream_len;
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "PutObject success. [%s:%s] tx_pkts:%d, tx_bytes:%d",
                                    bucket, object, data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
        }
        else
        {
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "PutObject success. [%s:%s]", bucket, object);
        }



        return true;
    }
    else
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "PutObject failed. [%s:%s]", bucket, object);

        if (hos_func->fs2_info[FS2_DATA_FLOW_STATE].fs2_handle && hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[FS2_DATA_FLOW_STATE].reserved;
            data_info->tx_failed_pkts[thread_id]++;
            data_info->tx_failed_bytes[thread_id] += stream_len;
        }

        return false;
    }
}

void hos_init_log()
{
    MESA_handle_runtime_log_creation("./log");
}

hos_instance hos_get_instance()
{
    if (g_hos_handle.S3Client != NULL)
    {
        g_hos_handle.count++;
        g_hos_instance.result = true;
        return &g_hos_instance;
    }
    g_hos_instance.result = false;
    return &g_hos_instance;
}

hos_instance hos_init_instance(const char *conf_path, const char *module, size_t thread_num, const char *bucket)
{
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    char hos_url[1024];

    if (conf_path == NULL || thread_num == 0)
    {
        g_hos_instance.result = false;
        g_hos_instance.error_code = HOS_PARAMETER_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "param error:conf_path:%s, thread_num:%lu", conf_path, thread_num);
        return &g_hos_instance;
    }

    MESA_load_profile_string_nodef(conf_path, module, "hos_serverip", hos_conf->ip, MAX_HOS_STRING_LEN);
	MESA_load_profile_uint_nodef(conf_path, module, "hos_serverport", &hos_conf->port);
	MESA_load_profile_string_nodef(conf_path, module, "hos_accesskeyid", hos_conf->accesskeyid, MAX_HOS_STRING_LEN);
	MESA_load_profile_string_nodef(conf_path, module, "hos_secretkey", hos_conf->secretkey, MAX_HOS_STRING_LEN);
    MESA_load_profile_string_def(conf_path, module, "hos_log_path", hos_conf->log_path, MAX_HOS_STRING_LEN, HOS_LOG_PATH);
    MESA_load_profile_uint_def(conf_path, module, "hos_log_level", &hos_conf->log_level, 30);
    MESA_load_profile_uint_def(conf_path, module, "hos_poolsize", &hos_conf->pool_thread_size, 0);
    MESA_load_profile_uint_def(conf_path, module, "hos_thread_sum", &hos_conf->thread_num, 32);
    MESA_load_profile_uint_def(conf_path, module, "hos_cache_size", &hos_conf->cache_size, 102400);
    MESA_load_profile_uint_def(conf_path, module, "hos_cache_count", &hos_conf->cache_count, 10);
    MESA_load_profile_uint_def(conf_path, module, "hos_fd_live_time_ms", &hos_conf->timeout, 1000);
    MESA_load_profile_string_nodef(conf_path, module, "hos_fs2_serverip", hos_conf->fs2_ip, INET6_ADDRSTRLEN);
    MESA_load_profile_uint_nodef(conf_path, module, "hos_fs2_serverport", &hos_conf->fs2_port);
    MESA_load_profile_string_def(conf_path, module, "hos_fs2_path", hos_conf->fs2_path, sizeof(hos_conf->fs2_path), "./hos_fs2.stat");
    MESA_load_profile_uint_def(conf_path, module, "hos_fs2_format", &hos_conf->fs2_fmt, 0);
    if (hos_conf->ip && hos_conf->port && strlen(hos_conf->accesskeyid) && strlen(hos_conf->secretkey))
    {
        g_hos_handle.log = MESA_create_runtime_log_handle(hos_conf->log_path, hos_conf->log_level);
        if (log == NULL)
        {
            g_hos_instance.result = false;
            g_hos_instance.error_code = HOS_RUNTIME_LOG_FAILED;
            snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "runtime log create failed.");
            return &g_hos_instance;
        }

        snprintf(hos_url, sizeof(hos_url), "http://%s:%d/hos/", hos_conf->ip, hos_conf->port);
        hos_client_create();
        if (g_hos_instance.result == true)
        {
            if(hos_verify_bucket(bucket) == false)
            {
                g_hos_instance.result = false;
                g_hos_instance.error_code = HOS_BUCKET_NOT_EXIST;
                snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "runtime log create failed.");
                MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "bucket:%s not exist.", bucket);
                hos_shutdown_instance();
                return &g_hos_instance;
            }
            g_hos_instance.hos_url_prefix = (const char *)calloc(1, strlen(hos_url) + 1);
            memcpy((void *)g_hos_instance.hos_url_prefix, hos_url, strlen(hos_url));
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "Instance init completed");
            if (hos_conf->fs2_ip && hos_conf->fs2_port)
            {
                hos_expand_fs2();
            }
            else
            {
                MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "hos fs2 function not starup");
            }
        }
        return &g_hos_instance;
    }
    else
    {
        g_hos_instance.result = false;
        g_hos_instance.error_code = HOS_CONF_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "hos param error:hos ip:%s, hos port:%u, accesskeyid:%s, secretkey:%s",
            hos_conf->ip, hos_conf->port, hos_conf->accesskeyid, hos_conf->secretkey);
        MESA_destroy_runtime_log_handle(g_hos_handle.log);
        return &g_hos_instance;
    }
}

int hos_create_bucket(const char *bucket)
{
    if ((bucket == NULL) || (g_hos_handle.S3Client == NULL))
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_create_bucket",
            "error:bucket:%s, s3client:%s", bucket, g_hos_handle.S3Client?"not null":"null");
        return HOS_PARAMETER_ERROR;
    }
    Aws::S3::S3Client& S3Client = *g_hos_handle.S3Client;

    /* 本地检查是否已经存在该bucket */
    for (Aws::S3::Model::Bucket& new_bucket : g_hos_handle.buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "%s was exits", bucket);
            return HOS_CLIENT_OK;
        }
    }

    Aws::S3::Model::CreateBucketRequest createBucketRequest;
    createBucketRequest.SetBucket(bucket);

    Aws::S3::Model::CreateBucketOutcome createBucketOutcome = S3Client.CreateBucket(createBucketRequest);

    if (!createBucketOutcome.IsSuccess())
    {
        Aws::S3::S3Errors errorcode = createBucketOutcome.GetError().GetErrorType();
        if (errorcode != Aws::S3::S3Errors::BUCKET_ALREADY_OWNED_BY_YOU)
        {
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,"error: %s create failed. %s",
                bucket, createBucketOutcome.GetError().GetMessage().c_str());
            return (int)errorcode + 1;
        }
    }
    MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "%s create successful", bucket);

    return HOS_CLIENT_OK;
}

static int hos_upload_stream(const char *bucket, const char *object, const char *data, size_t data_len,
    put_finished_callback callback, void *userdata, size_t thread_id)
{
    char buf[128];
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    int ret;
    int mode = 0;

    if ((g_hos_handle.S3Client == NULL) || (bucket == NULL) || (object == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_stream",
            "s3client:%s, bucket:%s, object:%s, thread_id:%d, thread_num:%d",
            g_hos_handle.S3Client?"not null":"null", bucket, object, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
    }

    mode = data?1:0; // 1, file mode; 0 buf mode

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(object);
    
    //设置上传数据类型
    if (mode == 0)
    {
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("hos_upload_file_tag", object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
    }
    else
    {
        //内存块
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::StringStream>("hos_upload_buf_tag");
        Aws::String stream (data, data_len);
        *input_data << stream;
        request.SetBody(input_data);
    }
    //field_stat2 record
    if (hos_func->fs2_info[0].fs2_handle)
    {
        if (hos_func->fs2_info[0].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[0].reserved;
            data_info->rx_pkts[thread_id]++;
            data_info->rx_bytes[thread_id] += data_len;
        }
    }

    //设置回调函数
    size_t fd = hash_get_min_free_fd(thread_id);
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu", thread_id, fd);
    context->SetUUID(buf);
    
    hos_fd_context_t info = {fd, 0, (char *)bucket, (char *)object, (void *)callback, userdata, NULL, 0, 0, 0 };
    add_fd_context(&g_fd_context[thread_id], &info);
    
    if (hos_conf->pool_thread_size > 0)
    {
        ret = hos_putobject_async(request, data_len, thread_id, fd, bucket, object);
    }
    else
    {
        ret = hos_putobject_sync(request, data_len, thread_id, fd, bucket, object);
    }

    if (ret == true)
    {
        return HOS_CLIENT_OK;
    }
    else
    {
        return HOS_SEND_FAILED;
    }
}

int hos_upload_file(const char *bucket, const char *file_path, put_finished_callback callback, void *userdata, size_t thread_id)
{
    struct stat buffer;

    if ((g_hos_handle.S3Client == NULL) || (bucket == NULL) || (file_path == NULL) || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file",
            "s3client:%s, bucket:%s, file_path:%s, thread_id:%d, thread_num:%d",
            g_hos_handle.S3Client?"not null":"null", bucket, file_path, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }

    if (stat(file_path, &buffer) == -1)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file", "The file:%s not exist", file_path);
        return HOS_FILE_NOT_EXIST;
    }
    return hos_upload_stream(bucket, file_path, NULL, buffer.st_size, callback, userdata, thread_id);
}

int hos_upload_buf(const char *bucket, const char *object, const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id)
{
    if ((g_hos_handle.S3Client == NULL) || (bucket == NULL) || (object == NULL) 
        || (buf == NULL) || (buf_len == 0)
        || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_buf",
            "s3client:%s, bucket:%s, object:%s, buf:%s, buf_len:%d, thread_id:%d, thread_num:%d",
            g_hos_handle.S3Client?"not null":"null", bucket, object,
            buf?"not null":"null", buf_len, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }
    return hos_upload_stream(bucket, object, buf, buf_len, callback, userdata, thread_id);
}

static void *hos_fd_manage(void *ptr)
{
    hos_fd_context_t *a_fd_context;
    size_t thread_sum = g_hos_handle.hos_config.thread_num;
    size_t thread_num;
    size_t fd;
    while(1)
    {
        if (g_hos_handle.hos_func.fd_thread_status)
            break;
        for (thread_num = 0; thread_num < thread_sum; thread_num++)
        {
            for(fd = 3; fd < MAX_HOS_CLIENT_FD_NUM + 1; fd++)
            {
                if (!g_fd_info[thread_num][fd])
                    continue;
                a_fd_context = find_context_by_fd(g_fd_context[thread_num], fd);
                if (!a_fd_context)
                    continue;
                if (a_fd_context->fd_status == HOS_FD_INJECT)
                {
                    if (a_fd_context->position == a_fd_context->recive_cnt)
                    {
                        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                            "[%s:%s] upload completed. [thread:%d fd:%d] delete",
                            a_fd_context->bucket, a_fd_context->object, thread_num, fd);
                        hos_delete_fd(fd, thread_num);
                    }
                    else if (a_fd_context->overtime <= get_current_ms())
                    {
                        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
                            "[%s:%s] upload not completed, but the live-time of [thread_id:%d fd:%d] is over.",
                            a_fd_context->bucket, a_fd_context->object, thread_num, fd);
                        hos_delete_fd(fd, thread_num);
                    }
                }
            }
        }
        usleep(500000);
    }
    pthread_exit(NULL);
}

int hos_open_fd(const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, int mode)
{
    if ((g_hos_handle.S3Client == NULL) || (bucket == NULL) || (object == NULL) || (thread_id > g_hos_handle.hos_config.thread_num) || strlen(bucket) == 0 || strlen(object) == 0)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_open_fd", 
            "parameter error:s3client:%s, bucket:%s, obejct:%s, thread_id:%s",
            g_hos_handle.S3Client, bucket, object, thread_id);
        return HOS_PARAMETER_ERROR;
    }

    size_t fd = hash_get_min_free_fd(thread_id);
    if (fd == 0)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_open_fd", 
            "error:fd not enough, thread_id:%d, fd free: %d, fd register:%d, fd inject:%d",
             thread_id,
             g_fd_info[thread_id][HOS_FD_FREE],
             g_fd_info[thread_id][HOS_FD_REGISTER],
             g_fd_info[thread_id][HOS_FD_INJECT]);
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, "hos_open_fd", "thread_id:%d, fd:%d", thread_id, fd);
        return HOS_FD_NOT_ENOUGH;
    }
    MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, "hos_open_fd", "thread_id:%d, fd:%d", thread_id, fd);
    
    hos_fd_context_t info = {fd, mode, (char *)bucket, (char *)object, (void *)callback, userdata, 
        NULL,/*cache*/ g_hos_handle.hos_config.cache_count, 0,/*position*/ 0,/*recive_cnt*/
        (long)g_hos_handle.hos_config.cache_size,/*cache_rest*/ HOS_FD_REGISTER,/*fd_status*/ 
        0,/*overtime*/ g_hos_handle.hos_config.timeout,};
    add_fd_context(&g_fd_context[thread_id], &info);

    {
        std::lock_guard<std::mutex> locker(m_client_lock);
        if (g_hos_handle.hos_func.fd_thread == 0)
        {
            g_hos_handle.hos_func.fd_thread_status = 0;
            pthread_create(&g_hos_handle.hos_func.fd_thread, NULL, hos_fd_manage, NULL);
        }
    }

    return fd;
}

int hos_write(size_t fd, const char *stream, size_t stream_len, size_t thread_id)
{
    struct stat buffer;
    hos_fd_context_t *a_fd_context = NULL;
    char num[128];
    int ret = 0;
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    size_t upload_len = 0;

    if ((fd < 3) || (stream == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, 
            "hos_write", "error: fd:%d, stream:%s, stream_len:%s, thread_id:%d.",
            fd, stream?"not null":"null", stream_len, thread_id);
        return HOS_PARAMETER_ERROR;
    }

    if (g_fd_info[thread_id][fd])
    {
        a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    }
    if (a_fd_context == NULL)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "fd info not find. thread_id:%d, fd:%d", thread_id, fd);
        return HOS_HASH_NOT_FIND;
    }
    MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "Get fd_context, thread_id:%d, fd:%d", thread_id, fd);
    
    //field_stat2 record
    if (hos_func->fs2_info[0].fs2_handle)
    {
        if (hos_func->fs2_info[0].reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info[0].reserved;
            data_info->rx_pkts[thread_id]++;
            data_info->rx_bytes[thread_id] += stream_len;
        }
    }

    // create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;

    //设置上传数据类型
    if (a_fd_context->mode & BUFF_MODE)
    {
        //BUFF_MODE
        if (a_fd_context->mode & APPEND_MODE)
        {
            //APPEND_MODE
            if (a_fd_context->cache == NULL)
            {
                //a_fd_context->cache = Aws::MakeShared<Aws::StringStream>("hos_write append mode");
                a_fd_context->cache = std::make_shared<Aws::StringStream>();
            }
            Aws::String buffer(stream, stream_len);
            *a_fd_context->cache << buffer;
            a_fd_context->cache_rest -= stream_len;
            data_info->cache[thread_id] += stream_len;
            if (a_fd_context->cache_count == 0 || --a_fd_context->cache_count)
            {
                //cache_count == 0,不设置cache_count的情况
                //cache_count > 0,设置cache_count的情况
                if (a_fd_context->cache_rest > 0)
                {
                    return HOS_CLIENT_OK;
                }
            }
            request.SetBody(a_fd_context->cache);

            // add headers
            atomic_add(&(a_fd_context->position), 1);
            snprintf(num, 128, "%lu", atomic_read(&(a_fd_context->position)));
            Aws::Map<Aws::String, Aws::String> headers;
            headers["x-hos-upload-type"] = "append";
            headers["x-hos-position"] = num;
            request.SetMetadata(headers);

            a_fd_context->cache->seekg(0, std::ios_base::end);
            upload_len = a_fd_context->cache->tellg();
            a_fd_context->cache->seekg(0, std::ios_base::beg);
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "x-hos-posotion:%s", num);
        }
        else
        {
            const std::shared_ptr<Aws::IOStream> input_data = 
                Aws::MakeShared<Aws::StringStream>("hos_write buffer mode");
            Aws::String buffer (stream, stream_len);
            *input_data << buffer;
            request.SetBody(input_data);
            upload_len = stream_len;
        }
    }
    else
    {
        if (stat(stream, &buffer) == -1)
        {
            MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "The file:%s not exist", stream);
            return HOS_FILE_NOT_EXIST;
        }
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("hos_write file mode", a_fd_context->object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
        upload_len = buffer.st_size;
    }

    request.SetBucket(a_fd_context->bucket);
    request.SetKey(a_fd_context->object);

    if (hos_conf->pool_thread_size > 0)
    {
        ret = hos_putobject_async(request, upload_len, thread_id, fd, a_fd_context->bucket, a_fd_context->object);
    }
    else
    {
        ret = hos_putobject_sync(request, upload_len, thread_id, fd, a_fd_context->bucket, a_fd_context->object);
    }

    //恢复fd 的cache设置
    if (a_fd_context->mode & APPEND_MODE)
    {
        a_fd_context->cache->seekg(0, std::ios_base::end);
        data_info->cache[thread_id] += upload_len;
        a_fd_context->cache->seekg(0, std::ios_base::beg);
        a_fd_context->cache.reset();
        a_fd_context->cache = NULL;
        a_fd_context->cache_rest = hos_conf->cache_size;
        a_fd_context->cache_count = hos_conf->cache_count;
    }
    
    if (ret == true)
    {
        return HOS_CLIENT_OK;
    }
    else
    {
        return HOS_SEND_FAILED;
    }
}

int hos_close_fd(size_t fd, size_t thread_id)
{
    hos_fd_context_t *a_fd_context = NULL;
    char num[128];
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    size_t upload_len = 0;

    if (fd < 3 || thread_id > hos_conf->thread_num)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_FATAL, "hos_close_fd",
            "error:fd:%d, thread_id:%d, thread_sum:%d.",
            fd, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
    }
    if (g_fd_info[thread_id][fd])
    {
        a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    }
    if (a_fd_context == NULL)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG,
            "hos_close_fd", "not find the a_fd_context of [fd:%d thread:%d]",
            fd, thread_id);
        return HOS_CLIENT_OK; 
    }

    //close fd 之前发送append的缓存中内容
    if ((a_fd_context->mode & BUFF_MODE) && (a_fd_context->mode & APPEND_MODE))
    {
        if (a_fd_context->cache_rest != (long)hos_conf->cache_size && a_fd_context->cache != NULL)
        {
            // Create and configure the asynchronous put object request.
            Aws::S3::Model::PutObjectRequest request;
            request.SetBucket(a_fd_context->bucket);
            request.SetKey(a_fd_context->object);
            request.SetBody(a_fd_context->cache);

            // add headers
            atomic_add(&(a_fd_context->position), 1);
            snprintf(num, 128, "%lu", atomic_read(&(a_fd_context->position)));
            Aws::Map<Aws::String, Aws::String> headers;
            headers["x-hos-upload-type"] = "append";
            headers["x-hos-position"] = num;
            request.SetMetadata(headers);
            a_fd_context->cache->seekg(0, std::ios_base::end);
            upload_len = a_fd_context->cache->tellg();
            a_fd_context->cache->seekg(0, std::ios_base::beg);

            if (hos_conf->pool_thread_size > 0)
            {
                hos_putobject_async(request, upload_len, thread_id, fd, a_fd_context->bucket, a_fd_context->object);
            }
            else
            {
                hos_putobject_sync(request, upload_len, thread_id, fd, a_fd_context->bucket, a_fd_context->object);
            }
        }
    }
    a_fd_context->fd_status = HOS_FD_INJECT;
    a_fd_context->cache.reset();
    a_fd_context->cache = NULL;
    a_fd_context->overtime = get_current_ms() + a_fd_context->timeout;
    a_fd_context->cache_rest = hos_conf->cache_size;
    a_fd_context->cache_count = hos_conf->cache_count;

    g_fd_info[thread_id][HOS_FD_REGISTER]--;
    g_fd_info[thread_id][HOS_FD_INJECT]++;

    return HOS_CLIENT_OK;
}

int hos_shutdown_instance()
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    size_t i = 0;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;

    if (g_hos_handle.S3Client == NULL)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, "hos_shutdown_instance", "There is no hos client.");
        return HOS_CLIENT_OK;
    }

    if (g_hos_handle.count > 0 && --g_hos_handle.count)
    {
        MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, "hos_shutdown_instance", "hos client count:%d.", g_hos_handle.count);
        return HOS_CLIENT_OK;
    }

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);

    if (hos_func->fd_thread)
    {
        hos_func->fd_thread_status = 1;
        pthread_join(hos_func->fd_thread, NULL);
    }
    if (hos_func->fs2_thread)
    {
        hos_func->fs2_status = HOS_FS2_STOP;
        pthread_join(hos_func->fs2_thread, NULL);
        for (i = 0; i < FS2_RECORD_EVENTS; i++)
        {
            screen_stat_handle_t *fs2_handle = &hos_func->fs2_info[i].fs2_handle;
            if (*fs2_handle)
            {
                FS_stop(fs2_handle);
                *fs2_handle = NULL;
            }
            if (hos_func->fs2_info[i].reserved)
            {
                if (i == 0)
                {
                #if 1
                    data_info_t * data_info = (data_info_t *)hos_func->fs2_info[i].reserved;
                    if (data_info->rx_pkts)
                        free(data_info->rx_pkts);
                    if (data_info->rx_bytes)
                        free(data_info->rx_bytes);
                    if (data_info->tx_pkts)
                        free(data_info->tx_pkts);
                    if (data_info->tx_bytes)
                        free(data_info->tx_bytes);
                    if (data_info->tx_failed_bytes)
                        free(data_info->tx_failed_bytes);
                    if (data_info->tx_failed_pkts);
                        free(data_info->tx_failed_pkts);
                    if (data_info->cache)
                        free(data_info->cache);
                #else
                    if (data_info->rx_pkts_last)
                        free(data_info->rx_pkts_last);
                    if (data_info->rx_bytes_last)
                        free(data_info->rx_bytes_last);
                    if (data_info->tx_pkts_last)
                        free(data_info->tx_pkts_last);
                    if (data_info->tx_bytes_last)
                        free(data_info->tx_bytes_last);
                #endif
                }
                free(hos_func->fs2_info[i].reserved);
                hos_func->fs2_info[i].reserved = NULL;
            }
            if (hos_func->fs2_info[i].line_ids)
            {
                free(hos_func->fs2_info[i].line_ids);
                hos_func->fs2_info[i].line_ids=NULL;
            }
            if (hos_func->fs2_info[i].column_ids)
            {
                free(hos_func->fs2_info[i].column_ids);
                hos_func->fs2_info[i].column_ids=NULL;
            }
        }
    }

    delete g_hos_handle.S3Client;
    g_hos_handle.S3Client = NULL;
    MESA_handle_runtime_log(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "delete s3client.");

    if (g_fd_info)
    {
        free(g_fd_info);
        g_fd_info = NULL;
    }

    for (i = 0; i < hos_conf->thread_num; i++)
    {
        delete_all(&g_fd_context[i]);
    }

    if (g_fd_context)
    {
        free(g_fd_context);
        g_fd_context = NULL;
    }

    Aws::ShutdownAPI(g_options);
    MESA_destroy_runtime_log_handle(g_hos_handle.log);
    g_hos_handle.log = NULL;
    memset(&g_hos_handle, 0 , sizeof(g_hos_handle));
    if (g_hos_instance.hos_url_prefix)
        free((void *)g_hos_instance.hos_url_prefix);
    memset(&g_hos_instance, 0, sizeof(g_hos_handle));

    return HOS_CLIENT_OK;
}
