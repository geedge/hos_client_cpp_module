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
#include <stdio.h>
}
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <fstream>
#include <iostream>
#include <aws/external/gtest.h>
#include <aws/testing/platform/PlatformTesting.h>
#include <aws/testing/TestingEnvironment.h>
#include <aws/testing/MemoryTesting.h>
#ifdef HOS_MOCK
#include "mock/hos_mock.h"
#endif
#include "hos_client.h"
#include "MESA_prof_load.h"
#include "hos_common.h"
#ifdef HOS_MESA_LOG
#include "MESA_handle_logger.h"
#else
#define RLOG_LV_DEBUG       10
#define RLOG_LV_INFO        20
#define RLOG_LV_FATAL       30

#define MESA_create_runtime_log_handle(path, lv) \
    (void *)fopen((path), "a+")
#define MESA_destroy_runtime_log_handle(handle) \
    fclose((FILE *)handle)
#define MESA_HANDLE_RUNTIME_LOG(handle, lv, mod, fmt, args...)   \
    do{   \
        fprintf(((FILE *) handle), "line:%d, level:%d, module:%s, ", __LINE__, lv, mod); \
        fprintf(((FILE *) handle), fmt, ##args);\
        fprintf(((FILE *) handle), "\n");\
    } while (0)
#endif

struct hos_instance_s g_hos_instance;
hos_client_handle_t g_hos_handle;//一个进程只允许有一个g_hos_handle
static std::mutex m_client_lock;
hos_fd_context_t **g_fd_context;
size_t *g_fd_info; //fd 实际从1开始,每个线程有独立的fd
static Aws::SDKOptions g_options;

static inline size_t get_current_ms()
{
    struct timespec timenow;
    clock_gettime(CLOCK_MONOTONIC, &timenow);
    return (timenow.tv_sec * 1000 + timenow.tv_nsec / 1000 / 1000 );
}   

static int hos_delete_fd(size_t thread_id, hos_fd_context_t *context)
{
    if (context == NULL)
    {
        return HOS_PARAMETER_ERROR;
    }
    
    if (context)
    {
        if (context->bucket)
        {
            free(context->bucket);
            context->bucket = NULL;
        }
        if (context->object)
        {
            free(context->object);
            context->object = NULL;
        }
        HASH_DEL(g_fd_context[thread_id], context);
        free(context);
    }
    
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
    a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
           "error: Not find the info of [thread_id:%lu fd:%lu]", thread_id, fd);

        if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info.reserved;
            data_info->tx_failed_pkts[thread_id]++;
            data_info->tx_failed_bytes[thread_id] += stream_len;
        }
    }
    else
    {
        bool result = outcome.IsSuccess();
        if (!result)
        {
            error = outcome.GetError().GetMessage().c_str();
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
                                    "error: [%s:%s] upload failed. error:%s", a_fd_context->bucket, a_fd_context->object, error);

            if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->tx_failed_pkts[thread_id]++;
                data_info->tx_failed_bytes[thread_id] += stream_len;
            }
        }
        else
        {
            if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->tx_pkts[thread_id]++;
                data_info->tx_bytes[thread_id] += stream_len;
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                        "debug: [%s:%s] upload success. tx_pkts:%lu, tx_bytes:%lu",
                                        a_fd_context->bucket, a_fd_context->object,
                                        data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
            }
            else
            {
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                        "debug: [%s:%s] upload success. stream size:%lu", a_fd_context->bucket, a_fd_context->object, stream_len);
            }
        }
        put_finished_callback callback = (put_finished_callback)a_fd_context->callback;
        callback(result, a_fd_context->bucket, a_fd_context->object, error, a_fd_context->userdata);
        if (a_fd_context->mode & APPEND_MODE)
        {
            //APPEND MODE 保留fd
            atomic_add(&(a_fd_context->recive_cnt), 1);
            if (a_fd_context->fd_status == HOS_FD_INJECT)
            {
                if (a_fd_context->position == a_fd_context->recive_cnt)
                {
                    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                            "debug: [%s:%s] upload completed. [thread:%lu fd:%lu] delete",
                                            a_fd_context->bucket, a_fd_context->object, thread_id, a_fd_context->fd);
                    hos_delete_fd(thread_id, a_fd_context);
                }
            }
        }
        else
        {
            //完整上传 删除fd
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: [%s:%s] upload completed. [thread:%lu fd:%lu] delete",
                                    a_fd_context->bucket, a_fd_context->object, thread_id, a_fd_context->fd);
            hos_delete_fd(thread_id, a_fd_context);
        }
    }
    g_hos_handle.task_num[thread_id]--;
    g_hos_handle.task_context[thread_id]--;
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
        //config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(hos_conf->pool_thread_size, Aws::Utils::Threading::OverflowPolicy::REJECT_IMMEDIATELY)); //支持线程池
        config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(hos_conf->pool_thread_size, Aws::Utils::Threading::OverflowPolicy::QUEUE_TASKS_EVENLY_ACCROSS_THREADS)); //支持线程池
    }
    else
    {
        //同步模式
    }

    #ifndef HOS_MOCK
    g_hos_handle.S3Client = new Aws::S3::S3Client(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    #else
    g_hos_handle.S3Client = new Aws::S3::S3ClientMock(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    #endif
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
        MESA_HANDLE_RUNTIME_LOG(log, RLOG_LV_FATAL, "hos_client_create", "error: %s", g_hos_instance.error_message);
        return;
    }

    g_hos_handle.buckets = outcome.GetResult().GetBuckets();
    g_hos_handle.count++;
    g_hos_handle.executor = std::dynamic_pointer_cast<Aws::Utils::Threading::PooledThreadExecutor>(config.executor);
    g_hos_handle.task_num = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    g_hos_handle.task_context = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));

    g_fd_context = (hos_fd_context_t **)calloc(hos_conf->thread_num, sizeof(hos_fd_context_t *));
    g_fd_info = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));

    #if 0
    if (g_hos_handle.hos_func.fd_thread == 0)
    {
        g_hos_handle.hos_func.fd_thread_status = 0;
        pthread_create(&g_hos_handle.hos_func.fd_thread, NULL, hos_fd_manage, NULL);
    }
    #endif

    MESA_HANDLE_RUNTIME_LOG(log, RLOG_LV_DEBUG, "hos_client_create", "debug: hos s3client create success, url:%s.",endpoint);
    g_hos_instance.result = true;
}

bool hos_verify_bucket(const char *bucket)
{
    if (bucket == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: bucket is null");
        return false;
    }   
    if (g_hos_instance.result != true || g_hos_handle.S3Client == NULL)
    {
        return false;
    }
    Aws::S3::Model::ListBucketsOutcome outcome = g_hos_handle.S3Client->ListBuckets();

    if (outcome.IsSuccess())
    {
        g_hos_handle.buckets = outcome.GetResult().GetBuckets();

        for (Aws::S3::Model::Bucket& new_bucket : g_hos_handle.buckets)
        {
            if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
            {
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_verify_bucket","debug: bucket:%s exits", bucket);
                return true;
            }
        }
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_verify_bucket","error:%s", outcome.GetError().GetMessage().c_str());
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
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    size_t task_num = 0;

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

        fs2_info = &hos_func->fs2_info;
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

            task_num += g_hos_handle.task_num[i];
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
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[4], FS_OP_SET, tx_failed_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[5], FS_OP_SET, tx_failed_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[6], FS_OP_SET, cache_interval);

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[4], FS_OP_SET, tx_failed_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[5], FS_OP_SET, tx_failed_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[6], FS_OP_SET, cache_sum);

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

    if (hos_func->fs2_info.fs2_handle)
        return;
    //data info
    /**********************************************************************************************************
     *              rx_pkts     rx_bytes    tx_pkts     tx_bytes    tx_failed_p     tx_failed_b     cache_bytes
     * current      10          100         1           100         0               0               100
     * total        100         1000        10          1000        0               0               100(无实意)
    ***********************************************************************************************************/
    fs2_info = &hos_func->fs2_info;
    hos_func->fs2_info.fs2_handle = hos_init_fs2((char *)"hos-data", strlen("hos-data"));
    fs2_handle = hos_func->fs2_info.fs2_handle;
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

    pthread_create(&hos_func->fs2_thread, NULL, fs2_statistics, NULL);

    return ;
}

static int hos_putobject_async(Aws::S3::Model::PutObjectRequest& request, size_t stream_len,
    size_t thread_id, size_t fd, const char *bucket, const char *object)
{
    char buf[128];
    int ret = 0;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;

    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu %lu", thread_id, fd, stream_len);
    context->SetUUID(buf);

    if (hos_conf->max_request_num && hos_conf->max_request_context && 
        (g_hos_handle.task_num[thread_id] >= hos_conf->max_request_num ||
        g_hos_handle.task_context[thread_id] >= hos_conf->max_request_context))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: PutObjectAsync failed. [%s:%s]. task_num:%lu, task_context:%lu",
            bucket, object, g_hos_handle.task_num[thread_id], g_hos_handle.task_context[thread_id]);

        return HOS_SEND_FAILED;
    }

    auto &S3Client = *(g_hos_handle.S3Client);
    ret = S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    if (ret)
    {
        g_hos_handle.task_num[thread_id]++;
        g_hos_handle.task_context[thread_id] += stream_len;
        //不算真正成功，需要等到PutObjectAsyncFinished的结果
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: PutObjectAsync success. [%s:%s]", bucket, object);
        
        return HOS_CLIENT_OK;
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: PutObjectAsync failed. [%s:%s]", bucket, object);

        if (hos_func->fs2_info.fs2_handle)
        {
            if (hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->tx_failed_pkts[thread_id]++;
                data_info->tx_failed_bytes[thread_id] += stream_len;
            }
        }
        return HOS_SEND_FAILED;
    }
}

static int hos_putobject_sync(Aws::S3::Model::PutObjectRequest& request, size_t stream_len, size_t thread_id, size_t fd,
    const char *bucket, const char *object)
{
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;

    auto& S3Client = *(g_hos_handle.S3Client);
    Aws::S3::Model::PutObjectOutcome Outcome = S3Client.PutObject(request);
    if (Outcome.IsSuccess())
    {
        if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info.reserved;
            data_info->tx_pkts[thread_id]++;
            data_info->tx_bytes[thread_id] += stream_len;
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: PutObject success. [%s:%s] tx_pkts:%lu, tx_bytes:%lu",
                                    bucket, object, data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
        }
        else
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: PutObject success. [%s:%s]", bucket, object);
        }

        return HOS_CLIENT_OK;
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: PutObject failed. [%s:%s] cause:%s", bucket, object, Outcome.GetError().GetMessage().c_str());

        if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info.reserved;
            data_info->tx_failed_pkts[thread_id]++;
            data_info->tx_failed_bytes[thread_id] += stream_len;
        }

        return (int)Outcome.GetError().GetErrorType() + 1;
    }
}

hos_instance hos_get_instance()
{
    if (g_hos_handle.S3Client != NULL)
    {
        g_hos_handle.count++;
        g_hos_instance.result = true;
        return &g_hos_instance;
    }
    memset(&g_hos_instance, 0, sizeof(g_hos_instance));
    g_hos_instance.result = false;
    return &g_hos_instance;
}

hos_instance hos_init_instance(const char *conf_path, const char *module, size_t thread_num, const char *bucket)
{
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    char hos_url[1024];

    if (conf_path == NULL || thread_num == 0 || module == NULL || bucket == NULL)
    {
        g_hos_instance.result = false;
        g_hos_instance.error_code = HOS_PARAMETER_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE,
            "param error:conf_path:%s, module:%s, thread_num:%lu, bucket:%s", conf_path, module, thread_num, bucket);
        return &g_hos_instance;
    }

    MESA_load_profile_string_nodef(conf_path, module, "hos_serverip", hos_conf->ip, MAX_HOS_STRING_LEN);
	MESA_load_profile_uint_nodef(conf_path, module, "hos_serverport", &hos_conf->port);
	MESA_load_profile_string_nodef(conf_path, module, "hos_accesskeyid", hos_conf->accesskeyid, MAX_HOS_STRING_LEN);
	MESA_load_profile_string_nodef(conf_path, module, "hos_secretkey", hos_conf->secretkey, MAX_HOS_STRING_LEN);
    MESA_load_profile_string_def(conf_path, module, "hos_log_path", hos_conf->log_path, MAX_HOS_STRING_LEN, HOS_LOG_PATH);
    MESA_load_profile_uint_def(conf_path, module, "hos_log_level", &hos_conf->log_level, 30);
    MESA_load_profile_uint_def(conf_path, module, "hos_poolsize", &hos_conf->pool_thread_size, 0);
    MESA_load_profile_uint_def(conf_path, module, "hos_cache_size", &hos_conf->cache_size, 102400);
    MESA_load_profile_uint_def(conf_path, module, "hos_cache_count", &hos_conf->cache_count, 10);
    MESA_load_profile_string_def(conf_path, module, "hos_fs2_serverip", hos_conf->fs2_ip, INET6_ADDRSTRLEN, "127.0.0.1");
    MESA_load_profile_uint_def(conf_path, module, "hos_fs2_serverport", &hos_conf->fs2_port, 10086);
    MESA_load_profile_string_def(conf_path, module, "hos_fs2_path", hos_conf->fs2_path, sizeof(hos_conf->fs2_path), "./hos_fs2.stat");
    MESA_load_profile_uint_def(conf_path, module, "hos_fs2_format", &hos_conf->fs2_fmt, 0);
    MESA_load_profile_uint_def(conf_path, module, "hos_request_num", &hos_conf->max_request_num, 100);
    MESA_load_profile_uint_def(conf_path, module, "hos_request_context", &hos_conf->max_request_context, 10240000);
    if (strlen(hos_conf->ip) && hos_conf->port && strlen(hos_conf->accesskeyid) && strlen(hos_conf->secretkey))
    {
        g_hos_handle.log = MESA_create_runtime_log_handle(hos_conf->log_path, hos_conf->log_level);
        if (g_hos_handle.log == NULL)
        {
            g_hos_instance.result = false;
            g_hos_instance.error_code = HOS_RUNTIME_LOG_FAILED;
            snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "runtime log create failed.");
            return &g_hos_instance;
        }

        snprintf(hos_url, sizeof(hos_url), "http://%s:%d/hos/", hos_conf->ip, hos_conf->port);
        hos_conf->thread_num = thread_num;
        hos_client_create();
        if (g_hos_instance.result == true)
        {
            if(hos_verify_bucket(bucket) == false)
            {
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: bucket:%s not exist.", bucket);
                hos_shutdown_instance();
                g_hos_instance.result = false;
                g_hos_instance.error_code = HOS_BUCKET_NOT_EXIST;
                snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "bucket:%s not exits.", bucket);
                return &g_hos_instance;
            }
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug:%s","Instance init completed");
            hos_expand_fs2();
            g_hos_instance.error_code = 0;
            g_hos_instance.error_message[0]='\0';
            g_hos_instance.hos_url_prefix = (const char *)calloc(1, strlen(hos_url) + 1);
            memcpy((void *)g_hos_instance.hos_url_prefix, hos_url, strlen(hos_url));
        }
        return &g_hos_instance;
    }
    else
    {
        g_hos_instance.result = false;
        g_hos_instance.error_code = HOS_CONF_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "hos param error:hos ip:%s, hos port:%u, accesskeyid:%s, secretkey:%s",
            hos_conf->ip, hos_conf->port, hos_conf->accesskeyid, hos_conf->secretkey);
        return &g_hos_instance;
    }
}

int hos_create_bucket(const char *bucket)
{
    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }
    if (bucket == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_create_bucket",
            "error:bucket:%s", bucket);
        return HOS_PARAMETER_ERROR;
    }
    auto& S3Client = *g_hos_handle.S3Client;

    /* 本地检查是否已经存在该bucket */
    for (Aws::S3::Model::Bucket& new_bucket : g_hos_handle.buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: %s was exits", bucket);
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
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,"error: %s create failed. %s",
                bucket, createBucketOutcome.GetError().GetMessage().c_str());
            return (int)errorcode + 1;
        }
    }
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: %s create successful", bucket);

    return HOS_CLIENT_OK;
}

static int hos_upload_stream(const char *bucket, const char *object, const char *data, size_t data_len,
    put_finished_callback callback, void *userdata, size_t thread_id)
{
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    int ret;
    int mode = 0;

    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if ((bucket == NULL) || (object == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_stream",
            "error: s3client:%s, bucket:%s, object:%s, thread_id:%lu, thread_num:%u",
            g_hos_handle.S3Client?"not null":"null", bucket, object, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
    }

    mode = data?1:0; // 0, file mode; 1 buf mode

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
    if (hos_func->fs2_info.fs2_handle)
    {
        if (hos_func->fs2_info.reserved)
        {
            data_info = (data_info_t *)hos_func->fs2_info.reserved;
            data_info->rx_pkts[thread_id]++;
            data_info->rx_bytes[thread_id] += data_len;
        }
    }

    //设置回调函数
    size_t fd = ++g_fd_info[thread_id];
    
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

    return ret;
}

int hos_upload_file(const char *bucket, const char *file_path, put_finished_callback callback, void *userdata, size_t thread_id)
{
    struct stat buffer;
    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if ((bucket == NULL) || (file_path == NULL) || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file",
            "error: bucket:%s, file_path:%s, thread_id:%lu, thread_num:%u",
            bucket, file_path, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }

    if (stat(file_path, &buffer) == -1)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file", "error: The file:%s not exist", file_path);
        return HOS_FILE_NOT_EXIST;
    }
    return hos_upload_stream(bucket, file_path, NULL, buffer.st_size, callback, userdata, thread_id);
}

int hos_upload_buf(const char *bucket, const char *object, const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id)
{
    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if ((bucket == NULL) || (object == NULL) || (buf == NULL) || (buf_len == 0)
        || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_buf",
            "bucket:%s, object:%s, buf:%s, buf_len:%lu, thread_id:%lu, thread_num:%u",
            bucket, object, buf?"not null":"null", buf_len, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }
    return hos_upload_stream(bucket, object, buf, buf_len, callback, userdata, thread_id);
}

int hos_open_fd(const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, int mode)
{
    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }
    if ((bucket == NULL) || (object == NULL) || (thread_id > g_hos_handle.hos_config.thread_num) || strlen(bucket) == 0 || strlen(object) == 0)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_open_fd", 
            "error: bucket:%s, obejct:%s, thread_id:%lu",
            (bucket == NULL)?"null":bucket, (object == NULL)?"null":object, thread_id);
        return HOS_PARAMETER_ERROR;
    }

    size_t fd = ++g_fd_info[thread_id];
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_open_fd", "debug: thread_id:%lu, fd:%lu", thread_id, fd);
    
    hos_fd_context_t info = {fd, mode, (char *)bucket, (char *)object, (void *)callback, userdata, 
        NULL,/*cache*/ g_hos_handle.hos_config.cache_count, 0,/*position*/ 0,/*recive_cnt*/
        (long)g_hos_handle.hos_config.cache_size,/*cache_rest*/ HOS_FD_REGISTER,/*fd_status*/};
    add_fd_context(&g_fd_context[thread_id], &info);

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

    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if ((stream == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, 
            "hos_write", "error: fd:%lu, stream:%s, stream_len:%lu, thread_id:%lu.",
            fd, stream?"not null":"null", stream_len, thread_id);
        return HOS_PARAMETER_ERROR;
    }

    a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: fd info not find. thread_id:%lu, fd:%lu", thread_id, fd);
        return HOS_HASH_NOT_FIND;
    }
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: Get fd_context, thread_id:%lu, fd:%lu", thread_id, fd);
    
    // create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;

    //设置上传数据类型
    if (a_fd_context->mode & BUFF_MODE)
    {
        //BUFF_MODE

        //field_stat2 record
        if (hos_func->fs2_info.fs2_handle)
        {
            if (hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->rx_pkts[thread_id]++;
                data_info->rx_bytes[thread_id] += stream_len;
            }
        }
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
            if (data_info != NULL)
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
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: x-hos-posotion:%s", num);
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
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: The file:%s not exist", stream);
            return HOS_FILE_NOT_EXIST;
        }
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("hos_write file mode", stream, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
        upload_len = buffer.st_size;
        //field_stat2 record
        if (hos_func->fs2_info.fs2_handle)
        {
            if (hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->rx_pkts[thread_id]++;
                data_info->rx_bytes[thread_id] += upload_len;
            }
        }
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
        if (data_info)
            data_info->cache[thread_id] -= upload_len;
        a_fd_context->cache.reset();
        a_fd_context->cache = NULL;
        a_fd_context->cache_rest = hos_conf->cache_size;
        a_fd_context->cache_count = hos_conf->cache_count;
    }

    return ret;
}

int hos_close_fd(size_t fd, size_t thread_id)
{
    hos_fd_context_t *a_fd_context = NULL;
    char num[128];
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    size_t upload_len = 0;

    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if (thread_id > hos_conf->thread_num)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_close_fd",
            "error:fd:%lu, thread_id:%lu, thread_sum:%u.",
            fd, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
    }
    a_fd_context = find_context_by_fd(g_fd_context[thread_id], fd);
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG,
            "hos_close_fd", "debug: not find the a_fd_context of [thread:%lu fd:%lu]",
            thread_id, fd);
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
            data_info_t *data_info = (data_info_t *)(g_hos_handle.hos_func.fs2_info.reserved);
            if (data_info)
                data_info->cache[thread_id] = 0;
        }
    }
    a_fd_context->fd_status = HOS_FD_INJECT;
    a_fd_context->cache.reset();
    a_fd_context->cache = NULL;
    a_fd_context->cache_rest = hos_conf->cache_size;
    a_fd_context->cache_count = hos_conf->cache_count;

    if (hos_conf->pool_thread_size == 0)
    {
        //同步模式，立即释放fd
        hos_delete_fd(thread_id, a_fd_context);
    }
    else
    {
        //异步APPEND 模式,判断是否可以释放
        //异步其他模式，在PutObjectAsyncFinished出释放fd
        if (a_fd_context->mode == (BUFF_MODE | APPEND_MODE) && a_fd_context->position == a_fd_context->recive_cnt)
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: [%s:%s] upload completed. [thread:%lu fd:%lu] delete",
                                    a_fd_context->bucket, a_fd_context->object, thread_id, a_fd_context->fd);
            hos_delete_fd(thread_id, a_fd_context);
        }
    }

    return HOS_CLIENT_OK;
}

int hos_shutdown_instance()
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    size_t i = 0;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    size_t task_num = 0;

    if (g_hos_instance.result == false || g_hos_handle.S3Client == NULL)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if (g_hos_handle.count > 0 && --g_hos_handle.count)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_shutdown_instance", "debug: hos client count:%lu.", g_hos_handle.count);
        return HOS_CLIENT_OK;
    }

    //先等待所有的task完成
    while(1)
    {
        task_num = 0;
        for (uint32_t i = 0; i < g_hos_handle.hos_config.thread_num; i++)
        {
            task_num += g_hos_handle.task_num[i];
        }
        if (task_num == 0)
            break;
        usleep(500 * 1000);
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
            screen_stat_handle_t *fs2_handle = &hos_func->fs2_info.fs2_handle;
            if (*fs2_handle)
            {
                FS_stop(fs2_handle);
                *fs2_handle = NULL;
            }
            if (hos_func->fs2_info.reserved)
            {
                data_info_t *data_info = (data_info_t *)hos_func->fs2_info.reserved;
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
                if (data_info->tx_failed_pkts)
                    ;
                free(data_info->tx_failed_pkts);
                if (data_info->cache)
                    free(data_info->cache);
                free(hos_func->fs2_info.reserved);
                hos_func->fs2_info.reserved = NULL;
            }
            if (hos_func->fs2_info.line_ids)
            {
                free(hos_func->fs2_info.line_ids);
                hos_func->fs2_info.line_ids=NULL;
            }
            if (hos_func->fs2_info.column_ids)
            {
                free(hos_func->fs2_info.column_ids);
                hos_func->fs2_info.column_ids=NULL;
            }
        }
    }

    delete g_hos_handle.S3Client;
    g_hos_handle.S3Client = NULL;
    if (g_hos_handle.task_num != NULL)
    {
        free(g_hos_handle.task_num);
        g_hos_handle.task_num = NULL;
    }
    if (g_hos_handle.task_context != NULL)
    {
        free(g_hos_handle.task_context);
        g_hos_handle.task_context = NULL;
    }
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: delete s3client.");

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
    memset(&g_hos_instance, 0, sizeof(g_hos_instance));

    return HOS_CLIENT_OK;
}
