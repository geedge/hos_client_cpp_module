/*************************************************************************
    > File Name: hos_client_api.cpp
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:00:23 PM CST
 ************************************************************************/
extern "C"
{
#include<string.h>
#include <sys/stat.h>
#include <unistd.h>
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

#define MAX_HOS_CLIENT_THREAD_NUM 255
#define MAX_HOS_CLIENT_FD_NUM 65535

#if(__GNUC__ * 100 + __GNUC_MINOR__ * 10 + __GNUC_PATCHLEVEL__ >= 410)
#define atomic_add(x,y) __sync_add_and_fetch((x),(y))
#define atomic_read(x) __sync_add_and_fetch((x),0)
#else 
#define atomic_add(x,y) ((*(x))+=(y))
#define atomic_read(x) 	(*(x))
#endif 

typedef struct data_info_s
{
    int *tx_pkts;
    int *tx_bytes;
    int *rx_pkts;
    int *rx_bytes;
    int *tx_pkts_last;
    int *tx_bytes_last;
    int *rx_pkts_last;
    int *rx_bytes_last;
}data_info_t;


typedef struct fs2_info_s
{
    screen_stat_handle_t fs2_handle;
    int *line_ids;
    int *column_ids;
    void *reserved; //预留给每个fs2 handle用来存储自定义的数据
}fs2_info_t;

typedef struct hos_client_handle_s
{
    Aws::S3::S3Client *S3Client;
    Aws::Vector<Aws::S3::Model::Bucket> buckets;
    std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor> executor;
    pthread_t fd_thread;
    int fd_thread_status;
    int count;  /* 记录了有多少个对象在使用hos */
    size_t pool_thread_size;
    /* options */
    size_t cache_size;
    size_t cache_count;
    size_t thread_sum;
    size_t timeout;
    /* expand */
#ifndef FS2_RECORD_EVENTS
#define FS2_RECORD_EVENTS 4
#endif
    fs2_info_t fs2_info[FS2_RECORD_EVENTS]; //0: data info; 1: fd info; 2 cache info; 3 PoolThread state
    pthread_t fs2_thread;
    int fs2_status;
#define HOS_FS2_START 1
#define HOS_FS2_STOP  2
}hos_client_handle_t;

std::mutex m_client_lock; /* create和destroy操作时使用的锁 */
hos_client_handle g_hos_handle;//一个进程只允许有一个g_hos_handle
//hos_info_t *hash_hos_info[MAX_HOS_CLIENT_THREAD_NUM];
hos_info_t **hash_hos_info;
size_t *hos_cache;//记录当前hos缓存了多少数据
size_t (*fd_info)[MAX_HOS_CLIENT_FD_NUM + 1]; //fd 实际从3开始， fd[thread_id][0]记录register的fd，fd[thread_id][1]记录inject的fd
Aws::SDKOptions g_options;
static char g_hos_error_msg[1024];
static size_t g_hos_error_num;

char *hos_get_error_msg()
{
    return g_hos_error_msg;
}

size_t hos_get_error_num()
{
    return g_hos_error_num;
}

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
        if (!fd_info[thread_id][i])
        {
            fd_info[thread_id][i] = 1;
            fd_info[thread_id][HOS_FD_REGISTER]++;
            fd_info[thread_id][HOS_FD_FREE]--;

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
    delete_info_by_fd(&hash_hos_info[thread_id], fd);
    fd_info[thread_id][fd] = 0;
    fd_info[thread_id][HOS_FD_FREE]++;
    fd_info[thread_id][HOS_FD_INJECT]--;
    
    return HOS_CLIENT_OK;
}

static void PutObjectAsyncFinished(const Aws::S3::S3Client* S3Client, 
    const Aws::S3::Model::PutObjectRequest& request, 
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    const char *error = NULL;
    hos_info_t *hos_info = NULL;
    bool result = outcome.IsSuccess();
    if (!result)
    {
        error = outcome.GetError().GetMessage().c_str();
    }
    const Aws::String& uuid = context->GetUUID();
    size_t thread_id, fd;
    sscanf(uuid.c_str(), "%lu %lu", &thread_id, &fd);
    if (fd_info[thread_id][fd])
    {
        hos_info = find_info_by_fd(hash_hos_info[thread_id], fd);
    }
    if (hos_info == NULL)
    {
        return ; 
    }
    put_finished_callback callback = (put_finished_callback)hos_info->callback;
    callback(result, hos_info->bucket, hos_info->object, error, hos_info->userdata);
    if (hos_info->mode & APPEND_MODE)
    {
        //APPEND MODE 保留fd
        atomic_add(&(hos_info->recive_cnt), 1);
    }else
    {
        //完整上传 删除fd
        hos_close_fd(fd, thread_id);
    }
}

void hos_set_cache_size(hos_client_handle client,  size_t cache_size)
{
    if (client == NULL)
    {
        return;
    }
    client->cache_size = cache_size;
    return ;
}

void hos_set_cache_count(hos_client_handle client, size_t cache_count)
{
    if (client == NULL)
    {
        return;
    }
    client->cache_count = cache_count;
    return ;
}

void hos_set_thread_sum(hos_client_handle client, size_t thread_sum)
{
    if (client == NULL)
    {
        return;
    }
    if (client->thread_sum >= thread_sum )
    {
        return;
    }

    for (size_t i = 0; i < thread_sum; i++)
    {
        fd_info[i][0] = 65533;
    }
    if (hos_cache)
    {
        hos_cache = (size_t *)realloc(hos_cache, thread_sum * sizeof(size_t));
    }
    if (hash_hos_info)
    {
        hash_hos_info = (hos_info_t **)realloc(hash_hos_info, thread_sum * sizeof(hos_info_t *));
        memset(&hash_hos_info[client->thread_sum], 0, (thread_sum - client->thread_sum) * sizeof(hos_info_t *));
    }
    else
    {
        hash_hos_info = (hos_info_t **)calloc(thread_sum, sizeof(hos_info_t*));
    }
    if (fd_info)
    {
        fd_info = (size_t (*) [MAX_HOS_CLIENT_FD_NUM + 1])realloc(fd_info, thread_sum * sizeof(size_t [MAX_HOS_CLIENT_FD_NUM + 1]));
    }else
    {
        fd_info = (size_t (*) [MAX_HOS_CLIENT_FD_NUM + 1])calloc(thread_sum, sizeof(size_t [MAX_HOS_CLIENT_FD_NUM + 1]));
    }
    client->thread_sum = thread_sum;
    return ;
}

hos_client_handle hos_client_create(const char *serverip, size_t port, const char *accesskeyid, const char *secretkey, size_t pool_size)
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    if (!serverip || !accesskeyid || !secretkey)
    {
        g_hos_error_num = HOS_PARAMETER_ERROR;
        snprintf(g_hos_error_msg, sizeof(g_hos_error_msg) - 1, "Error: Parameter is null. serverip:%s,port:%lu, accesskeyid:%s, secretkey:%s, pool_size:%lu\n",
            serverip, port, accesskeyid, secretkey, pool_size);
        return NULL;
    }

    if (g_hos_handle)
    {
        g_hos_handle->count++;
        return g_hos_handle;
    }

    Aws::InitAPI(g_options);
    g_hos_handle = (hos_client_handle)malloc(sizeof(hos_client_handle_t));
    memset(g_hos_handle, 0, sizeof(hos_client_handle_t));
    Aws::Client::ClientConfiguration config;
    Aws::Auth::AWSCredentials credentials(accesskeyid, secretkey);

    //初始化
    char endpoint[128];
    snprintf(endpoint, 128, "http://%s:%lu/hos/", serverip, port);
    config.endpointOverride = endpoint;
    config.verifySSL = false;
    config.enableEndpointDiscovery = true;
    config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(pool_size, Aws::Utils::Threading::OverflowPolicy::REJECT_IMMEDIATELY));//支持线程池

    g_hos_handle->S3Client = new Aws::S3::S3Client(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    /* 获取当前用户的所有的buckets */
    Aws::S3::Model::ListBucketsOutcome outcome = g_hos_handle->S3Client->ListBuckets();

    if (!outcome.IsSuccess())
    {
        delete g_hos_handle->S3Client;
        Aws::ShutdownAPI(g_options);
        free(g_hos_handle);
        g_hos_handle = NULL;
        g_hos_error_num = (size_t)outcome.GetError().GetErrorType() + 1;
        snprintf(g_hos_error_msg, sizeof(g_hos_error_msg) - 1, outcome.GetError().GetMessage().c_str());
        return NULL;
    }

    g_hos_handle->buckets = outcome.GetResult().GetBuckets();
    g_hos_handle->cache_size = 0;
    g_hos_handle->cache_count = 0;
    g_hos_handle->thread_sum = 1;
    g_hos_handle->timeout = 1000;
    g_hos_handle->count++;
    g_hos_handle->pool_thread_size = pool_size;
    g_hos_handle->executor = std::dynamic_pointer_cast<Aws::Utils::Threading::PooledThreadExecutor>(config.executor);

    hos_cache = (size_t *)calloc(g_hos_handle->thread_sum, sizeof(size_t));
    hash_hos_info = (hos_info_t **)calloc(1, sizeof(hos_info_t *));
    fd_info = (size_t (*)[MAX_HOS_CLIENT_FD_NUM + 1])calloc(1, sizeof(size_t [MAX_HOS_CLIENT_FD_NUM + 1]));

    fd_info[0][0] = 65533;
    fd_info[0][1] = 0;
    fd_info[0][2] = 0;

    return g_hos_handle;
}

static void *fs2_statistics(void *ptr)
{
    hos_client_handle handle = (hos_client_handle)ptr;
    size_t i = 0;
    int rx_pkts_sum = 0;
    int rx_bytes_sum = 0;
    int tx_pkts_sum = 0;
    int tx_bytes_sum = 0;
    int rx_pkts_sum_interval = 0;
    int rx_bytes_sum_interval = 0;
    int tx_pkts_sum_interval = 0;
    int tx_bytes_sum_interval = 0;
    fs2_info_t *fs2_info = NULL; 
    int PoolThread_state[4] = {0, 0, 0, 0};//{PoolSize, Busy, TopBusy, AveBusy}
    int *busy = &PoolThread_state[1];
    int *top_busy = &PoolThread_state[2];
    int *ave_busy = &PoolThread_state[3];
    int pool_history_sum = 0;
    size_t time = 0;

    PoolThread_state[0] = g_hos_handle->pool_thread_size;
    while(1)
    {
        if (handle->fs2_status == HOS_FS2_STOP)
        {
            break;
        }

        rx_pkts_sum = 0;
        rx_bytes_sum = 0;
        tx_pkts_sum = 0;
        tx_bytes_sum = 0;
        rx_pkts_sum_interval = 0;
        rx_bytes_sum_interval = 0;
        tx_pkts_sum_interval = 0;
        tx_bytes_sum_interval = 0;

        *busy = g_hos_handle->executor->GetTaskSize();
        *top_busy = (*busy) > (*top_busy) ? (*busy) : (*top_busy);
        pool_history_sum += *busy;
        time++;
        *ave_busy = pool_history_sum / time;

        //pkts and bytes info
        fs2_info = &handle->fs2_info[0];
        for (i = 0; i < handle->thread_sum; i++)
        {
            data_info_t *data_info = (data_info_t *)fs2_info->reserved;
            rx_pkts_sum += data_info->rx_pkts[i];
            rx_bytes_sum += data_info->rx_bytes[i];
            tx_pkts_sum += data_info->tx_pkts[i];
            tx_bytes_sum += data_info->tx_bytes[i];
            rx_pkts_sum_interval += (data_info->rx_pkts[i] - data_info->rx_pkts_last[i]);
            rx_bytes_sum_interval += (data_info->rx_bytes[i] - data_info->rx_bytes_last[i]);
            tx_pkts_sum_interval += (data_info->tx_pkts[i] - data_info->tx_pkts_last[i]);
            tx_bytes_sum_interval += (data_info->tx_bytes[i] - data_info->tx_bytes_last[i]);

            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i], fs2_info->column_ids[0], FS_OP_SET, data_info->rx_pkts[i]);
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i], fs2_info->column_ids[1], FS_OP_SET, data_info->rx_bytes[i]);
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i], fs2_info->column_ids[2], FS_OP_SET, data_info->tx_pkts[i]);
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i], fs2_info->column_ids[3], FS_OP_SET, data_info->tx_bytes[i]);

            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i + 1], fs2_info->column_ids[0], FS_OP_SET, (data_info->rx_pkts[i] - data_info->rx_pkts_last[i]));
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i + 1], fs2_info->column_ids[1], FS_OP_SET, (data_info->rx_bytes[i] - data_info->rx_bytes_last[i]));
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i + 1], fs2_info->column_ids[2], FS_OP_SET, (data_info->tx_pkts[i] - data_info->tx_pkts_last[i]));
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * i + 1], fs2_info->column_ids[3], FS_OP_SET, (data_info->tx_bytes[i] - data_info->tx_bytes_last[i]));

            data_info->rx_pkts_last[i] = data_info->rx_pkts[i];
            data_info->rx_bytes_last[i] = data_info->rx_bytes[i];
            data_info->tx_pkts_last[i] = data_info->tx_pkts[i];
            data_info->tx_bytes_last[i] = data_info->tx_bytes[i];
        }
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_sum);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_sum);

        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum + 1], fs2_info->column_ids[0], FS_OP_SET, rx_pkts_sum_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum + 1], fs2_info->column_ids[1], FS_OP_SET, rx_bytes_sum_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum + 1], fs2_info->column_ids[2], FS_OP_SET, tx_pkts_sum_interval);
        FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[2 * handle->thread_sum + 1], fs2_info->column_ids[3], FS_OP_SET, tx_bytes_sum_interval);

        //fd info
        fs2_info = &handle->fs2_info[1];
        for (i = 0; i < handle->thread_sum; i++)
        {
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[i], fs2_info->column_ids[0], FS_OP_SET, fd_info[i][1]);
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[i], fs2_info->column_ids[1], FS_OP_SET, fd_info[i][2]);
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[i], fs2_info->column_ids[2], FS_OP_SET, fd_info[i][0]);
        }

        //cache info
        fs2_info = &handle->fs2_info[2];
        for (i = 0; i < handle->thread_sum; i++)
        {
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[i], fs2_info->column_ids[0], FS_OP_SET, hos_cache[i]);
        }

        //PoolThread State
        fs2_info = &handle->fs2_info[3];
        for (i = 0; i < 4; i++)
        {
            FS_operate(fs2_info->fs2_handle, fs2_info->line_ids[0], fs2_info->column_ids[i], FS_OP_SET, PoolThread_state[i]);
        }
        
        sleep(1);
    }
    pthread_exit(NULL);
}

void hos_expand_fs2(hos_client_handle handle, const char * path, int format, char *server_ip, int port)
{
    fs2_info_t *fs2_info = NULL;
    screen_stat_handle_t *fs2_handle = NULL;
    const char *app_name = "hos-sdk-client-cpp";
    int value = 0;
    char buff[128];
    int i = 0;

    if (handle->fs2_info[0].fs2_handle)
        return;
    //fs2 init
    for (i = 0; i < FS2_RECORD_EVENTS; i++)
    {
        fs2_handle = &handle->fs2_info[i].fs2_handle;
        *fs2_handle = FS_create_handle();

        FS_set_para(*fs2_handle, APP_NAME, app_name, strlen(app_name) + 1);
        value = 1;//true
        FS_set_para(*fs2_handle, FLUSH_BY_DATE, &value, sizeof(value));
        if (path != NULL)
        {
            FS_set_para(*fs2_handle, OUTPUT_DEVICE, path, strlen(path) + 1);
        }
        value = 2;//append
        FS_set_para(*fs2_handle, PRINT_MODE, &value, sizeof(value));
        value = 1;
        FS_set_para(*fs2_handle, CREATE_THREAD, &value, sizeof(value));
        FS_set_para(*fs2_handle, METRIS_FORMAT, &format, sizeof(format));
        FS_set_para(*fs2_handle, STAT_CYCLE, &value, sizeof(value));
        value = 4096;
        FS_set_para(*fs2_handle, MAX_STAT_FIELD_NUM, &value, sizeof(value));
        if (server_ip == NULL)
        {
            FS_set_para(*fs2_handle, STATS_SERVER_IP, "127.0.0.1", strlen("127.0.0.1"));
        }else
        {
            FS_set_para(*fs2_handle, STATS_SERVER_IP, server_ip, strlen(server_ip));
        }

        FS_set_para(*fs2_handle, STATS_SERVER_PORT, &port, sizeof(port));

        value = FS_OUTPUT_STATSD;
        FS_set_para(*fs2_handle, STATS_FORMAT, &value, sizeof(value));
    }


    //pkts and bytes info
    fs2_info = &handle->fs2_info[0];
    fs2_handle = &handle->fs2_info[0].fs2_handle;
    fs2_info->line_ids = (int *)calloc(2 * handle->thread_sum + 2, sizeof(int));
    fs2_info->column_ids = (int *)calloc(4, sizeof(int));

    //line info
    snprintf(buff, sizeof(buff), "rx_pkts");
    fs2_info->column_ids[0] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "rx_bytes");
    fs2_info->column_ids[1] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "tx_pkts");
    fs2_info->column_ids[2] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "tx_bytes");
    fs2_info->column_ids[3] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    for (i = 0; i < (int)handle->thread_sum; i++)
    {
        snprintf(buff, sizeof(buff), "total(%d)", i);
        fs2_info->line_ids[2 * i] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
        snprintf(buff, sizeof(buff), "rate(%d)", i);
        fs2_info->line_ids[2 * i + 1] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
    }
    snprintf(buff, sizeof(buff), "total");
    fs2_info->line_ids[2 * handle->thread_sum] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "rate");
    fs2_info->line_ids[2 * handle->thread_sum + 1] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);

    handle->fs2_status = HOS_FS2_START;

    data_info_t *data_info = (data_info_t *)calloc(1, sizeof(data_info_t));
    fs2_info->reserved = (void *)data_info;
    data_info->tx_pkts = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->tx_bytes = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->rx_pkts = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->rx_bytes = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->tx_pkts_last = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->tx_bytes_last = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->rx_pkts_last = (int *)calloc(handle->thread_sum, sizeof(int));
    data_info->rx_bytes_last = (int *)calloc(handle->thread_sum, sizeof(int));
    FS_start(*fs2_handle);

    //fd info
    fs2_info = &handle->fs2_info[1];
    fs2_handle = &handle->fs2_info[1].fs2_handle;
    fs2_info->line_ids = (int *)calloc(handle->thread_sum, sizeof(int));
    fs2_info->column_ids = (int *)calloc(3, sizeof(int));

    snprintf(buff, sizeof(buff), "REGISTER");
    fs2_info->column_ids[0] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "INJECT");
    fs2_info->column_ids[1] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);
    snprintf(buff, sizeof(buff), "FREE");
    fs2_info->column_ids[2] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);

    for (i = 0; i < (int)handle->thread_sum; i++)
    {
        snprintf(buff, sizeof(buff), "num(%d)", i);
        fs2_info->line_ids[i] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
    }
    FS_start(*fs2_handle);

    //cache info
    fs2_info = &handle->fs2_info[2];
    fs2_handle = &handle->fs2_info[2].fs2_handle;
    fs2_info->line_ids = (int *)calloc(handle->thread_sum + 1, sizeof(int));
    fs2_info->column_ids = (int *)calloc(1, sizeof(int));

    snprintf(buff, sizeof(buff), "cached");
    fs2_info->column_ids[0] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, buff);

    for (i = 0; i < (int)handle->thread_sum; i++)
    {
        snprintf(buff, sizeof(buff), "Bytes(%d)", i);
        fs2_info->line_ids[i] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
    }
    snprintf(buff, sizeof(buff), "total");
    fs2_info->line_ids[i] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, buff);
    FS_start(*fs2_handle);

    //PoolThread state
    /*******************************************************
     *              PoolSize    Busy    TopBusy     AveBusy
     * ThreadNum    1000        500     800         650
    ********************************************************/
    fs2_info = &handle->fs2_info[3];
    fs2_handle = &handle->fs2_info[3].fs2_handle;
    fs2_info->line_ids = (int *)calloc(1, sizeof(int));
    fs2_info->column_ids = (int *)calloc(4, sizeof(int));

    const char *poolthread_col[4] = {"PoolSize", "Busy", "TopBusy", "AveBusy"};
    for (i = 0; i < 4; i++)
    {
        fs2_info->column_ids[i] = FS_register(*fs2_handle, FS_STYLE_COLUMN, FS_CALC_CURRENT, poolthread_col[i]);
    }
    fs2_info->line_ids[0] = FS_register(*fs2_handle, FS_STYLE_LINE, FS_CALC_CURRENT, "ThreadNum");

    FS_start(*fs2_handle);

    pthread_create(&handle->fs2_thread, NULL, fs2_statistics, handle);

    return ;
}

bool hos_verify_bucket(hos_client_handle handle, const char *bucket)
{
    Aws::S3::Model::ListBucketsOutcome outcome = handle->S3Client->ListBuckets();

    if (outcome.IsSuccess())
    {
        handle->buckets = outcome.GetResult().GetBuckets();

        for (Aws::S3::Model::Bucket& new_bucket : handle->buckets)
        {
            if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

int hos_create_bucket(hos_client_handle handle, const char *bucket)
{
    if ((bucket == NULL) || (handle == NULL))
    {
        return HOS_PARAMETER_ERROR;
    }
    Aws::S3::S3Client& S3Client = *handle->S3Client;

    /* 本地检查是否已经存在该bucket */
    for (Aws::S3::Model::Bucket& new_bucket : handle->buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
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
            return (int)errorcode + 1;
        }
    }

    return HOS_CLIENT_OK;
}

static int hos_upload_stream(hos_client_handle handle, const char *bucket, const char *object, 
        const char *data, size_t data_len, put_finished_callback callback, void *userdata, size_t thread_id, int file_type)
{
    struct stat buffer;
    char buf[128];
    size_t stream_len = 0;
    data_info_t *data_info = NULL;
    int ret;

    if ((handle == NULL) || (bucket == NULL) || (object == NULL) || (callback == NULL) || (thread_id > handle->thread_sum))
    {
        return HOS_PARAMETER_ERROR;
    }
    Aws::S3::S3Client& S3Client = *handle->S3Client;

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(object);
    
    //设置上传数据类型
    if (file_type == 0)
    {
        if (stat(data, &buffer) == -1)
        {
            return HOS_FILE_NOT_EXITS;
        }

        stream_len = buffer.st_size;
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag", object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
    }
    else
    {
        //内存块
        stream_len = data_len;
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::StringStream>(data);
        Aws::String stream (data, data_len);
        *input_data << stream;
        request.SetBody(input_data);
    }
    //field_stat2 record
    if (handle->fs2_info[0].fs2_handle)
    {
        if (handle->fs2_info[0].reserved)
        {
            data_info = (data_info_t *)handle->fs2_info[0].reserved;
            data_info->rx_pkts[thread_id]++;
            data_info->rx_bytes[thread_id] += stream_len;
        }
    }

    //设置回调函数
    size_t fd = hash_get_min_free_fd(thread_id);
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu", thread_id, fd);
    context->SetUUID(buf);
    
    hos_info_t info = {fd, 0, handle, (char *)bucket, (char *)object, (void *)callback, userdata, NULL, 0, 0, 0 };
    add_hos_info(&hash_hos_info[thread_id], &info);
    
    ret = S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    if (ret)
    {
        //field_stat2 record
        if (handle->fs2_info[0].fs2_handle)
        {
            if (handle->fs2_info[0].reserved)
            {
                data_info = (data_info_t *)handle->fs2_info[0].reserved;
                data_info->tx_pkts[thread_id]++;
                data_info->tx_bytes[thread_id] += stream_len;
            }
        }
        return HOS_CLIENT_OK;
    }
    return HOS_SEND_FAILED;
}

int hos_upload_file(hos_client_handle handle, const char *bucket, const char *file_path, 
        put_finished_callback callback, void *userdata, size_t thread_id)
{
    return hos_upload_stream(handle, bucket, file_path, NULL, 0, callback, userdata, thread_id, 0);
}

int hos_upload_buf(hos_client_handle handle, const char *bucket, const char *object, 
        const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id)
{
    return hos_upload_stream(handle, bucket, object, buf, buf_len, callback, userdata, thread_id, 1);
}

static void *hos_fd_manage(void *ptr)
{
    hos_info_t *hos_info;
    hos_client_handle handle = (hos_client_handle)ptr;
    size_t thread_sum = handle->thread_sum;
    size_t thread_num;
    size_t fd;
    while(1)
    {
        if (handle->fd_thread_status)
            break;
        for (thread_num = 0; thread_num < thread_sum; thread_num++)
        {
            for(fd = 3; fd < MAX_HOS_CLIENT_FD_NUM + 1; fd++)
            {
                if (!fd_info[thread_num][fd])
                    continue;
                hos_info = find_info_by_fd(hash_hos_info[thread_num], fd);
                if (!hos_info)
                    continue;
                if (hos_info->fd_status == HOS_FD_INJECT)
                {
                    if ((hos_info->position == hos_info->recive_cnt) || (hos_info->overtime <= get_current_ms()))
                        hos_delete_fd(fd, thread_num);
                }
            }
        }
        usleep(500000);
    }
    pthread_exit(NULL);
}

int hos_open_fd(hos_client_handle handle, const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, int mode)
{
    if ((handle == NULL) || (bucket == NULL) || (object == NULL) || (thread_id > handle->thread_sum))
    {
        return HOS_PARAMETER_ERROR;
    }

    size_t fd = hash_get_min_free_fd(thread_id);
    if (fd == 0)
    {
        return HOS_FD_NOT_ENOUGH;
    }
    
    hos_info_t info = {fd, mode, handle, (char *)bucket, (char *)object, (void *)callback, userdata, 
        NULL,/*cache*/ handle->cache_count, 0,/*position*/ 0,/*recive_cnt*/(long)handle->cache_size,/*cache_rest*/ HOS_FD_REGISTER,/*fd_status*/ 0,/*overtime*/ handle->timeout,};
    add_hos_info(&hash_hos_info[thread_id], &info);
#if 1
    if (handle->fd_thread == 0)
    {
        handle->fd_thread_status = 0;
        pthread_create(&handle->fd_thread, NULL, hos_fd_manage, handle);
    }
#endif
    return fd;
}

int hos_write(size_t fd, const char *stream, size_t stream_len, size_t thread_id)
{
    struct stat buffer;
    hos_info_t *hos_info = NULL;
    hos_client_handle handle = NULL;
    char num[128];
    char buf[128];
    int ret = 0;
    data_info_t *data_info = NULL;

    if ((fd == 0) || (stream == NULL) || (thread_id > g_hos_handle->thread_sum))
    {
        return HOS_PARAMETER_ERROR;
    }

    if (fd_info[thread_id][fd])
    {
        hos_info = find_info_by_fd(hash_hos_info[thread_id], fd);
    }
    if (hos_info == NULL)
    {
        return HOS_HASH_NOT_FIND;
    }

    handle = (hos_client_handle)hos_info->handle;
    
    //field_stat2 record
    if (handle->fs2_info[0].fs2_handle)
    {
        if (handle->fs2_info[0].reserved)
        {
            data_info = (data_info_t *)handle->fs2_info[0].reserved;
            data_info->rx_pkts[thread_id]++;
            data_info->rx_bytes[thread_id] += stream_len;
        }
    }

    Aws::S3::S3Client& S3Client = *(handle->S3Client);

    // create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;

    //设置上传数据类型
    if (hos_info->mode & BUFF_MODE)
    {
        //BUFF_MODE
        if (hos_info->mode & APPEND_MODE)
        {
            //APPEND_MODE
            if (hos_info->cache == NULL)
            {
                hos_info->cache = Aws::MakeShared<Aws::StringStream>("append mode");
            }
            if (hos_info->cache_count == 0)
            {
                //不设置cache_count的情况下
                Aws::String buffer (stream, stream_len);
                *hos_info->cache << buffer;
                hos_info->cache_rest -= stream_len;
                hos_cache[thread_id] += stream_len;
                if (hos_info->cache_rest > 0)
                {
                    return HOS_CLIENT_OK;
                }
            }else 
            {
                // cache
                Aws::String buffer (stream, stream_len);
                *hos_info->cache << buffer;
                hos_info->cache_rest -= stream_len;
                hos_cache[thread_id] += stream_len;
                //设置cache times的情况下
                if (--hos_info->cache_count)
                {
                    if (hos_info->cache_rest > 0)
                    {
                        return HOS_CLIENT_OK;
                    }
                }
            }
            request.SetBody(hos_info->cache);

            // add headers
            snprintf(num, 128, "%lu", ++hos_info->position);
            Aws::Map<Aws::String, Aws::String> headers;
            if (hos_info->mode & APPEND_MODE)
            {
                headers["x-hos-upload-type"] = "append";
                headers["x-hos-position"] = num;
                request.SetMetadata(headers);
            }
        }else
        {
            const std::shared_ptr<Aws::IOStream> input_data = 
                Aws::MakeShared<Aws::StringStream>("buffer mode");
            Aws::String buffer (stream, stream_len);
            *input_data << buffer;
            request.SetBody(input_data);
        }
    }
    else
    {
        if (stat(stream, &buffer) == -1)
        {
            return HOS_FILE_NOT_EXITS;
        }
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag", hos_info->object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
    }

    request.SetBucket(hos_info->bucket);
    request.SetKey(hos_info->object);

    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu", thread_id, fd);
    context->SetUUID(buf);

    ret = S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);

    hos_cache[thread_id] = 0;
    //恢复fd 的cache设置
    if (hos_info->mode & APPEND_MODE)
    {
        hos_info->cache.reset();
        hos_info->cache = NULL;
        hos_info->cache_rest = hos_info->handle->cache_size;
        hos_info->cache_count = hos_info->handle->cache_count;
    }
    if (ret)
    {
        if (data_info)
        {
            data_info->tx_pkts[thread_id]++;
            if (hos_info->mode & BUFF_MODE)
            {
                if (hos_info->mode & APPEND_MODE)
                {
                    data_info->tx_bytes[thread_id] += handle->cache_size;
                }else
                {
                    data_info->tx_bytes[thread_id] += stream_len;
                }
            }else
            {
                data_info->tx_bytes[thread_id] += buffer.st_size;
            }
        }
    }else
    {
        return HOS_SEND_FAILED;
    }
    
    return HOS_CLIENT_OK;
}

int hos_close_fd(size_t fd, size_t thread_id)
{
    hos_info_t *hos_info = NULL;
    char num[128];
    char buf[128];
    data_info_t *data_info = NULL;

    if (fd < 3)
    {
        return HOS_PARAMETER_ERROR;
    }
    if (fd_info[thread_id][fd])
    {
        hos_info = find_info_by_fd(hash_hos_info[thread_id], fd);
    }
    if (hos_info == NULL)
    {
        return HOS_CLIENT_OK; 
    }

    //close fd 之前发送append的缓存中内容
    if ((hos_info->mode & BUFF_MODE) && (hos_info->mode & APPEND_MODE))
    {
        if (hos_info->cache_rest != (long)hos_info->handle->cache_size)
        {
            //handle = (hos_client_handle)hos_info->handle;
            Aws::S3::S3Client& S3Client = *(hos_info->handle->S3Client);

            // Create and configure the asynchronous put object request.
            Aws::S3::Model::PutObjectRequest request;
            request.SetBucket(hos_info->bucket);
            request.SetKey(hos_info->object);
            request.SetBody(hos_info->cache);

            // add headers
            snprintf(num, 128, "%lu", ++hos_info->position);
            Aws::Map<Aws::String, Aws::String> headers;
            headers["x-hos-upload-type"] = "append";
            headers["x-hos-position"] = num;
            request.SetMetadata(headers);

            std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
                Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
            sprintf(buf, "%lu %lu", thread_id, fd);
            context->SetUUID(buf);

            S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);

            if (hos_info->handle->fs2_info[0].fs2_handle)
            {
                if (hos_info->handle->fs2_info[0].reserved)
                    data_info = (data_info_t *)hos_info->handle->fs2_info[0].reserved;

                data_info->tx_pkts[thread_id]++;
                data_info->tx_bytes[thread_id] += hos_info->handle->cache_size - hos_info->cache_rest;
            }
            hos_cache[thread_id] = 0;
        }
    }
    hos_info->fd_status = HOS_FD_INJECT;
    hos_info->cache.reset();
    hos_info->overtime = get_current_ms() + hos_info->timeout;

    fd_info[thread_id][HOS_FD_REGISTER]--;
    fd_info[thread_id][HOS_FD_INJECT]++;

    return HOS_CLIENT_OK;
}

int hos_client_destory(hos_client_handle handle)
{
    std::lock_guard<std::mutex> locker(m_client_lock);
    size_t i = 0;
    if (handle == NULL)
    {
        return HOS_PARAMETER_ERROR;
    }

    if (--handle->count)
    {
        return HOS_CLIENT_OK;
    }

    Aws::Vector<Aws::S3::Model::Bucket>().swap(handle->buckets);

    if (handle->fd_thread)
    {
        handle->fd_thread_status = 1;
        pthread_join(handle->fd_thread, NULL);
    }
    if (handle->fs2_thread)
    {
        handle->fs2_status = HOS_FS2_STOP;
        pthread_join(handle->fs2_thread, NULL);
        for (i = 0; i < FS2_RECORD_EVENTS; i++)
        {
            screen_stat_handle_t *fs2_handle = &handle->fs2_info[i].fs2_handle;
            FS_stop(fs2_handle);
            if (handle->fs2_info[i].reserved)
            {
                if (i == 0)
                {
                    data_info_t * data_info = (data_info_t *)handle->fs2_info[i].reserved;
                    if (data_info->rx_pkts)
                        free(data_info->rx_pkts);
                    if (data_info->rx_bytes)
                        free(data_info->rx_bytes);
                    if (data_info->tx_pkts)
                        free(data_info->tx_pkts);
                    if (data_info->tx_bytes)
                        free(data_info->tx_bytes);
                    if (data_info->rx_pkts_last)
                        free(data_info->rx_pkts_last);
                    if (data_info->rx_bytes_last)
                        free(data_info->rx_bytes_last);
                    if (data_info->tx_pkts_last)
                        free(data_info->tx_pkts_last);
                    if (data_info->tx_bytes_last)
                        free(data_info->tx_bytes_last);
                }
                free(handle->fs2_info[i].reserved);
            }
            if (handle->fs2_info[i].line_ids)
                free(handle->fs2_info[i].line_ids);
            if (handle->fs2_info[i].column_ids)
                free(handle->fs2_info[i].column_ids);
        }
    }

    delete handle->S3Client;

    if (hos_cache)
    {
        free(hos_cache);
    }

    if (fd_info)
    {
        free(fd_info);
    }

    for (i = 0; i < handle->thread_sum; i++)
    {
        delete_all(&hash_hos_info[i]);
    }

    if (hash_hos_info)
    {
        free(hash_hos_info);
    }

    free(handle);
    g_hos_handle = NULL;

    Aws::ShutdownAPI(g_options);

    return HOS_CLIENT_OK;
}
