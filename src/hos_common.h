#ifndef __HOS_COMMON_H__
#define __HOS_COMMON_H__

#include <netinet/in.h>
#include <mutex>
#include "field_stat2.h"
#include "hos_hash.h"
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/threading/Executor.h>

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
    #ifndef HOS_MOCK
    Aws::S3::S3Client *S3Client;
    #else
    Aws::S3::S3ClientMock *S3Client;
    #endif
    Aws::Vector<Aws::S3::Model::Bucket> buckets;
    std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor> executor;
    size_t count;  /* 记录了有多少个对象在使用hos */
    hos_config_t hos_config;
    hos_func_thread_t hos_func;
    void *log;
    size_t *task_num;
}hos_client_handle_t;

extern struct hos_instance_s g_hos_instance;
extern hos_client_handle_t g_hos_handle;//一个进程只允许有一个g_hos_handle
extern hos_fd_context_t **g_fd_context;
extern size_t (*g_fd_info)[MAX_HOS_CLIENT_FD_NUM + 1]; //fd 实际从3开始， fd[thread_id][0]记录register的fd，fd[thread_id][1]记录inject的fd

#endif