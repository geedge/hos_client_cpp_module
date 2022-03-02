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
static std::mutex m_instance_lock;
static std::mutex m_delete_lock;
static Aws::SDKOptions g_options;
Aws::Auth::AWSCredentials g_credentials;
Aws::Client::ClientConfiguration *g_client_config;

static int hos_delete_fd(size_t fd, size_t thread_id)
{
    hos_fd_context_t* context = (hos_fd_context_t *)fd;
    if (context == NULL)
    {
        return HOS_PARAMETER_ERROR;
    }

    put_finished_callback callback = (put_finished_callback)context->callback;
    if (callback)
    {
        callback(context->reslut, context->bucket, context->object, context->error, context->errorcode, context->userdata);
    }
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
    free(context);

    return HOS_CLIENT_OK;
}

static void PutObjectAsyncFinished(const Aws::S3::S3Client* S3Client, 
    const Aws::S3::Model::PutObjectRequest& request, 
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    hos_fd_context_t *a_fd_context = NULL;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    const Aws::String& uuid = context->GetUUID();
    size_t thread_id, fd, stream_len;

    sscanf(uuid.c_str(), "%zu %zu %zu", &thread_id, &fd, &stream_len);
    a_fd_context = (hos_fd_context_t *)fd;
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
           "error: [%s] fd is NULL", g_hos_instance.hos_url_prefix);

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
        a_fd_context->reslut = result;
        if (!result)
        {
            a_fd_context->error = outcome.GetError().GetMessage().c_str();
            a_fd_context->errorcode = (size_t)outcome.GetError().GetErrorType() + 1;
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,
                                    "error: [%s/%s/%s] upload failed. error:%s", g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, a_fd_context->error);

            if (hos_func->fs2_info.fs2_handle && hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->tx_failed_pkts[thread_id]++;
                data_info->tx_failed_bytes[thread_id] += stream_len;
            }

            put_finished_callback callback = (put_finished_callback)a_fd_context->callback;
            if (callback)
            {
                callback(a_fd_context->reslut, a_fd_context->bucket, a_fd_context->object, a_fd_context->error, a_fd_context->errorcode, a_fd_context->userdata);
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
                                        "debug: [%s/%s/%s] upload success. tx_pkts:%zu, tx_bytes:%zu",
                                        g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object,
                                        data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
            }
            else
            {
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                        "debug: [%s/%s/%s] upload success. stream size:%zu", g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, stream_len);
            }
            a_fd_context->error = NULL;
            a_fd_context->errorcode = 0;
        }

        if (a_fd_context->mode & APPEND_MODE)
        {
            std::lock_guard<std::mutex> locker(m_delete_lock);
            //APPEND MODE 保留fd
            atomic_add(&(a_fd_context->recive_cnt), 1);
            if (a_fd_context->fd_status == HOS_FD_CANCEL)
            {
                if (a_fd_context->position == a_fd_context->recive_cnt)
                {
                    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                            "debug: [%s/%s/%s] upload completed. [thread:%zu fd:%zu] delete",
                                            g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, thread_id, fd);
                    hos_delete_fd(fd, thread_id);
                }
            }
        }
        else
        {
            //完整上传 删除fd
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: [%s/%s/%s] upload completed. [thread:%zu fd:%zu] delete",
                                    g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, thread_id, fd);
            hos_delete_fd(fd, thread_id);
        }
    }
    atomic_sub(&g_hos_handle.task_num[thread_id], 1);
    atomic_sub(&g_hos_handle.task_context[thread_id], stream_len);
}

static int hos_attempt_connection()
{
    /* 获取当前用户的所有的buckets */
    Aws::S3::Model::ListBucketsOutcome outcome = g_hos_handle.S3Client->ListBuckets();

    if (!outcome.IsSuccess())
    {
        g_hos_instance.error_code = (size_t)outcome.GetError().GetErrorType() + 1;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, outcome.GetError().GetMessage().c_str());
        atomic_set(&g_hos_instance.status, INSTANCE_ATTEMPT_STATE);
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "[%s] ErrorCode:%d, Error: %s",
            g_hos_instance.hos_url_prefix, g_hos_instance.error_code, g_hos_instance.error_message);

        if (g_hos_instance.error_code == NETWORK_CONNECTION)
        {
            atomic_set(&g_hos_instance.status, INSTANCE_ATTEMPT_STATE);
        }
        else
        {
            atomic_set(&g_hos_handle.count, 0);//立即shutdown
            g_hos_instance.status =  INSTANCE_DISABLE_STATE;
            g_hos_instance.error_code = (size_t)outcome.GetError().GetErrorType() + 1;
            snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, outcome.GetError().GetMessage().c_str());
        }
        return g_hos_instance.error_code;
    }
    #if 0
    //不主动验证bucket
    //验证bucket存不存在
    for (Aws::S3::Model::Bucket &new_bucket : g_hos_handle.buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), g_hos_handle.hos_config.bucket) == 0)
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "[%s] debug: bucket:%s exits",
                                    g_hos_instance.hos_url_prefix, g_hos_handle.hos_config.bucket);
            bucket_exit = true;
        }
    }
    if (bucket_exit == false)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "[%s]error: bucket:%s not exist.",
                                g_hos_instance.hos_url_prefix, g_hos_handle.hos_config.bucket);
        char url[1024];
        snprintf(url, 1024, g_hos_instance.hos_url_prefix);
        char bucket[1024];
        snprintf(bucket, 1024, g_hos_handle.hos_config.bucket);
        hos_shutdown_instance();
        atomic_set(&g_hos_handle.count, 0); //立即shutdown
        g_hos_instance.error_code = HOS_BUCKET_NOT_EXIST;
        snprintf(g_hos_instance.error_message, 1024, "[%s] error: bucket %s not exits.", url, bucket);
        return HOS_BUCKET_NOT_EXIST;
    }
    g_hos_handle.buckets = outcome.GetResult().GetBuckets();
    #endif
    atomic_set(&g_hos_instance.status, INSTANCE_ENABLE_STATE);
    return HOS_CLIENT_OK;
}

//检测hos 是否可用
static void *hos_attempt_connection_exhaustively(void *ptr)
{
    while(atomic_read(&g_hos_handle.hos_func.hos_client_retry_thread_status) == 0)
    {
        hos_attempt_connection();
        sleep(g_hos_handle.hos_config.reconnection_time);
    }
    pthread_exit(NULL);
}

static void *fs2_statistics(void *ptr)
{
    size_t rx_pkts_last = 0;
    size_t rx_bytes_last = 0;
    size_t tx_pkts_last = 0;
    size_t tx_bytes_last = 0;
    size_t tx_failed_bytes_last = 0;
    size_t tx_failed_pkts_last = 0;
    size_t cache_last = 0;
    size_t req_overflow_last = 0;
    fs2_info_t *fs2_info = NULL; 
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;

    while(1)
    {
        if (hos_func->fs2_status == HOS_FS2_STOP)
        {
            break;
        }

        //pkts and bytes info
        size_t rx_pkts_sum = 0;
        size_t rx_bytes_sum = 0;
        size_t tx_pkts_sum = 0;
        size_t tx_bytes_sum = 0;
        size_t tx_failed_bytes_sum = 0;
        size_t tx_failed_pkts_sum = 0;
        size_t cache_sum = 0;

        fs2_info = &hos_func->fs2_info;
        data_info_t *data_info = (data_info_t *)fs2_info->reserved;
        for (uint32_t i = 0; i < hos_conf->thread_num; i++)
        {
            rx_pkts_sum += data_info->rx_pkts[i];
            rx_bytes_sum += data_info->rx_bytes[i];
            tx_pkts_sum += data_info->tx_pkts[i];
            tx_bytes_sum += data_info->tx_bytes[i];
            tx_failed_bytes_sum += data_info->tx_failed_bytes[i];
            tx_failed_pkts_sum += data_info->tx_failed_pkts[i];
            cache_sum += data_info->cache[i];
        }
        size_t req_overflow_sum = atomic_read(&data_info->tx_req_num_overflow);

        size_t rx_pkts_interval = rx_pkts_sum - rx_pkts_last;
        size_t rx_bytes_interval = rx_bytes_sum - rx_bytes_last;
        size_t tx_pkts_interval = tx_pkts_sum - tx_pkts_last;
        size_t tx_bytes_interval = tx_bytes_sum - tx_bytes_last;
        size_t tx_failed_pkts_interval = tx_failed_pkts_sum - tx_failed_pkts_last;
        size_t tx_failed_bytes_interval = tx_failed_bytes_sum - tx_failed_bytes_last;
        size_t cache_interval = cache_sum - cache_last;
        size_t req_overflow_interval = req_overflow_sum - req_overflow_last;

        rx_pkts_last = rx_pkts_sum;
        rx_bytes_last = rx_bytes_sum;
        tx_pkts_last = tx_pkts_sum;
        tx_bytes_last = tx_bytes_sum;
        tx_failed_bytes_last = tx_failed_bytes_sum;
        tx_failed_pkts_last = tx_failed_pkts_sum;
        cache_last = cache_sum;
        req_overflow_last = req_overflow_sum;

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[4], FS_OP_SET, tx_failed_pkts_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[5], FS_OP_SET, tx_failed_bytes_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[6], FS_OP_SET, cache_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[7], FS_OP_SET, req_overflow_interval);

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[4], FS_OP_SET, tx_failed_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[5], FS_OP_SET, tx_failed_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[6], FS_OP_SET, cache_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[1], fs2_info->column_ids[7], FS_OP_SET, req_overflow_sum);

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
    //value = 0; //true
    //FS_set_para(fs2_handle, FLUSH_BY_DATE, &value, sizeof(value));
    if (hos_conf->fs2_path != NULL)
    {
        FS_set_para(fs2_handle, OUTPUT_DEVICE, hos_conf->fs2_path, strlen(hos_conf->fs2_path) + 1);
    }
    value = 1;
    FS_set_para(fs2_handle, PRINT_MODE, &value, sizeof(value));
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
     *              rx_pkts     rx_bytes    tx_pkts     tx_bytes    tx_failed_p     tx_failed_b     cache_bytes  req_overflow
     * current      10          100         1           100         0               0               100          0
     * total        100         1000        10          1000        0               0               100(无实意)   0
    ***********************************************************************************************************/
    fs2_info = &hos_func->fs2_info;
    hos_func->fs2_info.fs2_handle = hos_init_fs2((char *)"hos-data", strlen("hos-data"));
    fs2_handle = hos_func->fs2_info.fs2_handle;
    fs2_info->line_ids = (int *)calloc(2, sizeof(int));
    fs2_info->column_ids = (int *)calloc(8, sizeof(int));

    const char *data_col[] = {"rx_pkts", "rx_bytes", "tx_pkts", "tx_bytes", "tx_failed_p", "tx_failed_b", "cache_bytes", "req_overflow"};
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

static void hos_client_create()
{
    hos_config_t *hos_conf = &g_hos_handle.hos_config;

    Aws::InitAPI(g_options);
    g_client_config = new Aws::Client::ClientConfiguration();
    g_credentials.SetAWSAccessKeyId(hos_conf->accesskeyid);
    g_credentials.SetAWSSecretKey(hos_conf->secretkey);

    //初始化
    char endpoint[128];
    snprintf(endpoint, 128, "http://%s:%u/hos/", hos_conf->ip, hos_conf->port);
    g_client_config->endpointOverride.append(endpoint);
    g_client_config->verifySSL = false;
    g_client_config->enableEndpointDiscovery = true;
    if (hos_conf->pool_thread_size > 0)
    {
        //异步模式
        //config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(hos_conf->pool_thread_size, Aws::Utils::Threading::OverflowPolicy::REJECT_IMMEDIATELY)); //支持线程池
        g_client_config->executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(hos_conf->pool_thread_size, Aws::Utils::Threading::OverflowPolicy::QUEUE_TASKS_EVENLY_ACCROSS_THREADS)); //支持线程池
    }
    else
    {
        //同步模式
    }

    #ifndef HOS_MOCK
    g_hos_handle.S3Client = new Aws::S3::S3Client(g_credentials, *g_client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    #else
    g_hos_handle.S3Client = new Aws::S3::S3ClientMock(g_credentials, *g_client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    #endif

    g_hos_instance.hos_url_prefix = g_client_config->endpointOverride.c_str();
    //hos 检测服务端是否可以连接上
    int ret = hos_attempt_connection();
    if (ret != HOS_CLIENT_OK && ret != NETWORK_CONNECTION)
    {
        return;
    }

    atomic_add(&g_hos_handle.count, 1);
    g_hos_handle.task_num = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));
    g_hos_handle.task_context = (size_t *)calloc(hos_conf->thread_num, sizeof(size_t));

    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_INFO, __FUNCTION__, "[%s] debug: hos s3client create success.",g_hos_instance.hos_url_prefix);
    hos_expand_fs2();


    if (g_hos_handle.hos_func.hos_client_retry_thread_id == 0)
    {
        g_hos_handle.hos_func.hos_client_retry_thread_status = 0;
        pthread_create(&g_hos_handle.hos_func.hos_client_retry_thread_id, NULL, hos_attempt_connection_exhaustively, NULL);
    }
}

bool hos_verify_bucket(const char *bucket)
{
    if (bucket == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: [%s] bucket is null", g_hos_instance.hos_url_prefix);
        return false;
    }   
    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
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
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_verify_bucket","debug: [%s] bucket:%s exits",g_hos_instance.hos_url_prefix, bucket);
                return true;
            }
            else
            {
                MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_verify_bucket","debug: [%s] Get bucket list:%s", g_hos_instance.hos_url_prefix, new_bucket.GetName().c_str());
            }
        }
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_verify_bucket","error:[%s] %s", g_hos_instance.hos_url_prefix, outcome.GetError().GetMessage().c_str());
    }
    return false;
}

static int hos_putobject_async(Aws::S3::Model::PutObjectRequest& request, size_t stream_len, size_t thread_id, hos_fd_context_t **fd)
{
    char buf[128];
    int ret = 0;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    char *bucket = (*fd)->bucket;
    char *object = (*fd)->object;

    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%zu %zu %zu", thread_id, (size_t)*fd, stream_len);
    context->SetUUID(buf);

    if (hos_conf->max_request_num && hos_conf->max_request_context && 
        (atomic_read(&g_hos_handle.task_num[thread_id]) >= hos_conf->max_request_num ||
        atomic_read(&g_hos_handle.task_context[thread_id]) >= hos_conf->max_request_context))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: [%s/%s/%s] PutObjectAsync failed. task_num:%lu, task_context:%lu", g_hos_instance.hos_url_prefix,
            bucket, object, atomic_read(&g_hos_handle.task_num[thread_id]), atomic_read(&g_hos_handle.task_context[thread_id]));

        if (hos_func->fs2_info.fs2_handle)
        {
            if (hos_func->fs2_info.reserved)
            {
                data_info = (data_info_t *)hos_func->fs2_info.reserved;
                data_info->tx_failed_pkts[thread_id]++;
                data_info->tx_failed_bytes[thread_id] += stream_len;
                atomic_add(&data_info->tx_req_num_overflow, 1);
            }
        }        

        atomic_sub(&((*fd)->position), 1);

        return HOS_SEND_FAILED;
    }

    auto &S3Client = *(g_hos_handle.S3Client);
    ret = S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    if (ret)
    {
        atomic_add(&g_hos_handle.task_num[thread_id], 1);
        atomic_add(&g_hos_handle.task_context[thread_id], stream_len);
        //不算真正成功，需要等到PutObjectAsyncFinished的结果
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: [%s/%s/%s] PutObjectAsync success.", g_hos_instance.hos_url_prefix, bucket, object);
        
        return HOS_CLIENT_OK;
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: [%s/%s/%s] PutObjectAsync failed.", g_hos_instance.hos_url_prefix, bucket, object);

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

static int hos_putobject_sync(Aws::S3::Model::PutObjectRequest& request, size_t stream_len, size_t thread_id, hos_fd_context_t **fd)
{
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    data_info_t *data_info = NULL;
    char *bucket = (*fd)->bucket;
    char *object = (*fd)->object;

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
                                    "debug: [%s/%s/%s] PutObject success. tx_pkts:%zu, tx_bytes:%zu",
                                    g_hos_instance.hos_url_prefix, bucket, object, data_info->tx_pkts[thread_id], data_info->tx_bytes[thread_id]);
        }
        else
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: [%s/%s/%s] PutObject success.", g_hos_instance.hos_url_prefix, bucket, object);
        }

        return HOS_CLIENT_OK;
    }
    else
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
            "debug: [%s/%s/%s] PutObject failed. cause:%s", g_hos_instance.hos_url_prefix, bucket, object, Outcome.GetError().GetMessage().c_str());

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
    switch (atomic_read(&g_hos_instance.status))
    {
    case INSTANCE_UNINIT_STATE:
        return NULL;
    default:
        atomic_add(&g_hos_handle.count, 1);
        return &g_hos_instance;
    }
}

int hos_get_init_instance_errorcode()
{
    return g_hos_instance.error_code; 
}

const char *hos_get_init_instance_errormsg()
{
    return g_hos_instance.error_message;
}

const char *hos_get_upload_endpoint()
{
    return g_hos_instance.hos_url_prefix;
}

hos_instance hos_init_instance(const char *conf_path, const char *module, size_t thread_num)
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    hos_config_t *hos_conf = &g_hos_handle.hos_config;

    memset(&g_hos_handle, 0, sizeof(g_hos_handle));
    memset(&g_hos_instance, 0, sizeof(g_hos_instance));

    if (conf_path == NULL || thread_num == 0 || module == NULL)
    {
        g_hos_instance.error_code = HOS_PARAMETER_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE,
            "param error:conf_path:%s, module:%s, thread_num:%zu", (conf_path?conf_path:"NULL"), (module?module:"NULL"), thread_num);
        return NULL;
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
    MESA_load_profile_uint_def(conf_path, module, "hos_reconnect_time", &hos_conf->reconnection_time, 1);
    MESA_load_profile_uint_def(conf_path, module, "hos_max_position", &hos_conf->max_position, 100000);
    if (strlen(hos_conf->ip) && hos_conf->port && strlen(hos_conf->accesskeyid) && strlen(hos_conf->secretkey))
    {
        g_hos_handle.log = MESA_create_runtime_log_handle(hos_conf->log_path, hos_conf->log_level);
        if (g_hos_handle.log == NULL)
        {
            g_hos_instance.error_code = HOS_RUNTIME_LOG_FAILED;
            snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "runtime log create failed.");
            return NULL;
        }

        hos_conf->thread_num = thread_num;
        hos_client_create();
        if (atomic_read(&g_hos_instance.status) == INSTANCE_DISABLE_STATE)
        {
            hos_shutdown_instance();
            return NULL;
        }
        return &g_hos_instance;
    }
    else
    {
        g_hos_instance.error_code = HOS_CONF_ERROR;
        snprintf(g_hos_instance.error_message, HOS_ERROR_MESSAGE_SIZE, "hos param error:hos ip:%s, hos port:%u, accesskeyid:%s, secretkey:%s",
            hos_conf->ip, hos_conf->port, hos_conf->accesskeyid, hos_conf->secretkey);
        return NULL;
    }
}

int hos_create_bucket(const char *bucket)
{
    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        return HOS_INSTANCE_NOT_ENABLE;
    }
    if (bucket == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_create_bucket",
            "error: [%s] bucket:%s", g_hos_instance.hos_url_prefix, "NULL");
        return HOS_PARAMETER_ERROR;
    }
    auto& S3Client = *g_hos_handle.S3Client;

    /* 本地检查是否已经存在该bucket */
    for (Aws::S3::Model::Bucket& new_bucket : g_hos_handle.buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: [%s] %s was exits", g_hos_instance.hos_url_prefix, bucket);
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
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__,"error: [%s] %s create failed. %s",
                g_hos_instance.hos_url_prefix, bucket, createBucketOutcome.GetError().GetMessage().c_str());
            return (int)errorcode + 1;
        }
    }
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: [%s] %s create successful", g_hos_instance.hos_url_prefix, bucket);

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

    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if ((bucket == NULL) || (object == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_stream",
            "error: [%s] s3client:%s, bucket:%s, object:%s, thread_id:%zu, thread_num:%u",
            g_hos_instance.hos_url_prefix, g_hos_handle.S3Client?"not null":"null", bucket, object, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
    }

    mode = data?1:0; // 0, file mode; 1 buf mode

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(object);
    
    //设置上传数据类型
    if (mode == FILE_MODE)
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
    hos_fd_context_t *hos_fd = (hos_fd_context_t *)calloc(1, sizeof(hos_fd_context_t));
    hos_fd->mode = mode;
    hos_fd->bucket = (char *)malloc(strlen(bucket) + 1);
    memcpy(hos_fd->bucket, bucket, strlen(bucket) + 1);
    hos_fd->object = (char *)malloc(strlen(object) + 1);
    memcpy(hos_fd->object, object, strlen(object) + 1);
    hos_fd->callback = (void *)callback;
    hos_fd->userdata = userdata;

    if (hos_conf->pool_thread_size > 0)
    {
        ret = hos_putobject_async(request, data_len, thread_id, &hos_fd);
    }
    else
    {
        ret = hos_putobject_sync(request, data_len, thread_id, &hos_fd);
    }

    return ret;
}

int hos_upload_file(const char *bucket, const char *file_path, put_finished_callback callback, void *userdata, size_t thread_id)
{
    struct stat buffer;
    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        return HOS_INSTANCE_NOT_ENABLE;
    }

    if ((bucket == NULL) || (file_path == NULL) || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file",
            "error: [%s] bucket:%s, file_path:%s, thread_id:%zu, thread_num:%u",
            g_hos_instance.hos_url_prefix, (bucket == NULL)?"NULL":bucket, (file_path == NULL)?"NULL":file_path, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }

    if (stat(file_path, &buffer) == -1)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_file", "error: [%s] The file:%s not exist", g_hos_instance.hos_url_prefix, file_path);
        return HOS_FILE_NOT_EXIST;
    }
    return hos_upload_stream(bucket, file_path, NULL, buffer.st_size, callback, userdata, thread_id);
}

int hos_upload_buf(const char *bucket, const char *object, const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id)
{
    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        return HOS_INSTANCE_NOT_ENABLE;
    }

    if ((bucket == NULL) || (object == NULL) || (buf == NULL) || (buf_len == 0)
        || (thread_id > g_hos_handle.hos_config.thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_upload_buf",
            "error:[%s] bucket:%s, object:%s, buf:%s, buf_len:%zu, thread_id:%zu, thread_num:%u",
            g_hos_instance.hos_url_prefix, bucket?bucket:"NULL", object?object:"NULL", buf?"not null":"null", buf_len, thread_id, g_hos_handle.hos_config.thread_num);
        return HOS_PARAMETER_ERROR;
    }
    return hos_upload_stream(bucket, object, buf, buf_len, callback, userdata, thread_id);
}

int hos_open_fd(const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, size_t *fd)
{
    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        *fd = 0;
        return HOS_INSTANCE_NOT_ENABLE;
    }
    if ((bucket == NULL) || (object == NULL) || (thread_id > g_hos_handle.hos_config.thread_num) || strlen(bucket) == 0 || strlen(object) == 0)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_open_fd", 
            "error: [%s] bucket:%s, obejct:%s, thread_id:%zu", g_hos_instance.hos_url_prefix,
            (bucket == NULL)?"null":bucket, (object == NULL)?"null":object, thread_id);
        *fd = 0;
        return HOS_PARAMETER_ERROR;
    }

    hos_fd_context_t *hos_fd = (hos_fd_context_t *)calloc(1, sizeof(hos_fd_context_t));
    hos_fd->mode = BUFF_MODE | APPEND_MODE;
    hos_fd->bucket = (char *)malloc(strlen(bucket) + 1);
    memcpy(hos_fd->bucket, bucket, strlen(bucket) + 1);
    hos_fd->object = (char *)malloc(strlen(object) + 1);
    memcpy(hos_fd->object, object, strlen(object) + 1);
    hos_fd->callback = (void *)callback;
    hos_fd->userdata = userdata;
    hos_fd->cache_count = g_hos_handle.hos_config.cache_count;
    hos_fd->cache_rest = g_hos_handle.hos_config.cache_size;
    hos_fd->fd_status = HOS_FD_REGISTER;
    hos_fd->reslut = true;
    hos_fd->thread_id = thread_id;

    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_open_fd", "debug: [%s] thread_id:%zu, fd:%ld", g_hos_instance.hos_url_prefix, thread_id, (long)hos_fd);

    *fd = (size_t)hos_fd;
    return HOS_CLIENT_OK;
}

int hos_write(size_t fd, const char *stream, size_t stream_len)
{
    hos_fd_context_t *a_fd_context = NULL;
    char num[128];
    int ret = 0;
    data_info_t *data_info = NULL;
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;
    size_t thread_id = 0;

    if (g_hos_instance.status != INSTANCE_ENABLE_STATE)
    {
        return HOS_INSTANCE_NOT_ENABLE;
    }

    a_fd_context = (hos_fd_context_t *)fd;
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, __FUNCTION__, "error: [%s] fd is NULL", g_hos_instance.hos_url_prefix);
        return HOS_FD_IS_INVALID;
    }
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: [%s] Get fd_context", g_hos_instance.hos_url_prefix);

    thread_id = a_fd_context->thread_id;
    if ((stream == NULL) || (thread_id > hos_conf->thread_num))
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, 
            "hos_write", "error: [%s] fd:%zu, stream:%s, stream_len:%zu, thread_id:%zu.",
            g_hos_instance.hos_url_prefix, fd, stream?"not null":"null", stream_len, thread_id);
        return HOS_PARAMETER_ERROR;
    }

    if (a_fd_context->position >= hos_conf->max_position)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, 
            __FUNCTION__, "error: [%s%s/%s] upload times over max times[%u] ",
            g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, hos_conf->max_position);

        return HOS_FD_OVER_POSITION;
    }
    // create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;

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
            return HOS_IN_CACHE;
        }
    }
    request.SetBody(a_fd_context->cache);

    // add headers
    atomic_add(&(a_fd_context->position), 1);
    snprintf(num, 128, "%zu", atomic_read(&(a_fd_context->position)));
    Aws::Map<Aws::String, Aws::String> headers;
    headers["x-hos-upload-type"] = "append";
    headers["x-hos-position"] = num;
    request.SetMetadata(headers);

    a_fd_context->cache->seekg(0, std::ios_base::end);
    size_t upload_len = a_fd_context->cache->tellg();
    a_fd_context->cache->seekg(0, std::ios_base::beg);
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: [%s] x-hos-posotion:%s", g_hos_instance.hos_url_prefix, num);
    request.SetBucket(a_fd_context->bucket);
    request.SetKey(a_fd_context->object);

    if (hos_conf->pool_thread_size > 0)
    {
        ret = hos_putobject_async(request, upload_len, thread_id, &a_fd_context);
    }
    else
    {
        ret = hos_putobject_sync(request, upload_len, thread_id, &a_fd_context);
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

int hos_close_fd(size_t fd)
{
    hos_fd_context_t *a_fd_context = NULL;
    char num[128];
    hos_config_t *hos_conf = &g_hos_handle.hos_config;
    size_t thread_id = 0;
    int ret = 0;

    if (g_hos_instance.status == INSTANCE_UNINIT_STATE)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    a_fd_context = (hos_fd_context_t *)fd;
    if (a_fd_context == NULL)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG,
            "hos_close_fd", "debug: [%s] not find the a_fd_context of [thread:%zu fd:%zu]",
            g_hos_instance.hos_url_prefix, thread_id, fd);
        return HOS_CLIENT_OK; 
    }

    thread_id = a_fd_context->thread_id;    
    if (thread_id > hos_conf->thread_num)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_FATAL, "hos_close_fd",
            "error: [%s] fd:%zu, thread_id:%zu, thread_sum:%u.",
            g_hos_instance.hos_url_prefix, fd, thread_id, hos_conf->thread_num);
        return HOS_PARAMETER_ERROR;
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
            snprintf(num, 128, "%zu", atomic_read(&(a_fd_context->position)));
            Aws::Map<Aws::String, Aws::String> headers;
            headers["x-hos-upload-type"] = "append";
            headers["x-hos-position"] = num;
            request.SetMetadata(headers);
            a_fd_context->cache->seekg(0, std::ios_base::end);
            size_t upload_len = a_fd_context->cache->tellg();
            a_fd_context->cache->seekg(0, std::ios_base::beg);

            if (hos_conf->pool_thread_size > 0)
            {
                ret = hos_putobject_async(request, upload_len, thread_id, &a_fd_context);
            }
            else
            {
                ret = hos_putobject_sync(request, upload_len, thread_id, &a_fd_context);
            }
            data_info_t *data_info = (data_info_t *)(g_hos_handle.hos_func.fs2_info.reserved);
            if (data_info)
                data_info->cache[thread_id] = 0;
        }
    }
    a_fd_context->cache.reset();
    a_fd_context->cache = NULL;
    a_fd_context->cache_rest = hos_conf->cache_size;
    a_fd_context->cache_count = hos_conf->cache_count;

    if (hos_conf->pool_thread_size == 0)
    {
        //同步模式，立即释放fd
        hos_delete_fd(fd, thread_id);
    }
    else
    {
        //异步APPEND 模式,判断是否可以释放
        //异步其他模式，在PutObjectAsyncFinished出释放fd
        std::lock_guard<std::mutex> locker(m_delete_lock);
        a_fd_context->fd_status = HOS_FD_CANCEL;
        if (a_fd_context->mode == (BUFF_MODE | APPEND_MODE) && a_fd_context->position == a_fd_context->recive_cnt)
        {
            MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__,
                                    "debug: [%s/%s/%s] upload completed. [thread:%zu fd:%zu] delete",
                                    g_hos_instance.hos_url_prefix, a_fd_context->bucket, a_fd_context->object, thread_id, fd);
            hos_delete_fd(fd, thread_id);
        }
    }

    if (ret == HOS_CLIENT_OK)
    {
        return HOS_CLIENT_OK;
    }
    else
    {
        return HOS_FD_CLOSE_BUT_SEND_FAILED;
    }
}

int hos_shutdown_instance()
{
    std::lock_guard<std::mutex> locker(m_instance_lock);
    hos_func_thread_t *hos_func = &g_hos_handle.hos_func;

    if (atomic_read(&g_hos_instance.status) == INSTANCE_UNINIT_STATE)
    {
        return HOS_INSTANCE_NOT_INIT;
    }

    if (g_hos_handle.count > 0 && --g_hos_handle.count)
    {
        MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, "hos_shutdown_instance", "debug: [%s] hos client count:%zu.", g_hos_instance.hos_url_prefix, g_hos_handle.count);
        return HOS_CLIENT_OK;
    }    
    
    //退出hos服务检测线程
    if (g_hos_handle.hos_func.hos_client_retry_thread_id != 0)
    {
        atomic_set(&g_hos_handle.hos_func.hos_client_retry_thread_status, 1);
        pthread_join(g_hos_handle.hos_func.hos_client_retry_thread_id, NULL);
    }

    //先等待所有的task完成
    while(1)
    {
        size_t task_num = 0;
        if (g_hos_handle.task_num == NULL)
        {
            break;
        }
        for (uint32_t i = 0; i < g_hos_handle.hos_config.thread_num; i++)
        {
            task_num += atomic_read(&g_hos_handle.task_num[i]);
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
        for (int i = 0; i < FS2_RECORD_EVENTS; i++)
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
    MESA_HANDLE_RUNTIME_LOG(g_hos_handle.log, RLOG_LV_DEBUG, __FUNCTION__, "debug: [%s] delete s3client.", g_hos_instance.hos_url_prefix);

    Aws::ShutdownAPI(g_options);
    MESA_destroy_runtime_log_handle(g_hos_handle.log);
    g_hos_handle.log = NULL;
    memset(&g_hos_handle, 0 , sizeof(g_hos_handle));
    memset(&g_hos_instance, 0, sizeof(g_hos_instance));

    return HOS_CLIENT_OK;
}
