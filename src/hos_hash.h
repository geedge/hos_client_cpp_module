/*************************************************************************
    > File Name: hos_hash.h
    > Author: pxz
    > Created Time: Fri 18 Sep 2020 05:00:04 PM CST
 ************************************************************************/
#ifndef __HOS_HASH_H__
#define __HOS_HASH_H__

#include <aws/core/Aws.h>
#include "hos_client.h"
#include "uthash.h"

typedef struct hos_info_s
{
    size_t fd;
    int  mode;
    hos_client_handle handle; 
    char *bucket;
    char *object;
    void *callback;
    void *userdata;
    std::shared_ptr<Aws::IOStream> cache;
    size_t cache_times;
    size_t cache_rest;
    size_t position;
    size_t recive_cnt;
    int fd_status;
#define HOS_FD_REGISTER 0
#define HOS_FD_INJECT 1
    size_t overtime;    //计算后超时的时间
    size_t timeout;     //配置的超时时间，从status变成INJECT开始计时
    UT_hash_handle hh;
}hos_info_t;

void add_hos_info(hos_info_t **handle, hos_info_t *input);
hos_info_t *find_info_by_fd(hos_info_t *handle, size_t fd);
void delete_info_by_fd(hos_info_t **handle, size_t fd);
void delete_all(hos_info_t **handle);
#endif
