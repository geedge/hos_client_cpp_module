#ifndef __GTEST_HOS_COMMON_H__
#define __GTEST_HOS_COMMON_H__

#include<stdint.h>
#include <netinet/in.h>

#ifndef MAX_HOS_STRING_LEN
#define MAX_HOS_STRING_LEN 1024
#endif

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

typedef struct hos_client_handle_s
{
    void *S3Client;
    void *buckets;
    void *executor;
    size_t count;  /* 记录了有多少个对象在使用hos */
    hos_config_t hos_config;
}hos_client_handle_t;

typedef unsigned long size_t;
typedef struct hos_client_handle_s hos_client_handle_t;
typedef struct hos_fd_context_s hos_fd_context_t;

extern struct hos_instance_s g_hos_instance;
extern hos_client_handle_t g_hos_handle;//一个进程只允许有一个g_hos_handle

#endif