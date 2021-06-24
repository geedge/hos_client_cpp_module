/*************************************************************************
    > File Name: hos_hash.h
    > Author: pxz
    > Created Time: Fri 18 Sep 2020 05:00:04 PM CST
 ************************************************************************/
#ifndef __HOS_HASH_H__
#define __HOS_HASH_H__

#include <aws/core/Aws.h>
//#include "hos_client.h"
#include "uthash.h"

typedef struct hos_fd_context_s
{
    size_t fd;
    int  mode;
    //hos_client_handle handle; 
    char *bucket;
    char *object;
    void *callback;
    void *userdata;
    std::shared_ptr<Aws::IOStream> cache;
    size_t cache_count;
    size_t position;
    size_t recive_cnt;
    long cache_rest;
    int fd_status;
#define HOS_FD_REGISTER 0
#define HOS_FD_INJECT 1
    UT_hash_handle hh;
}hos_fd_context_t;

void add_fd_context(hos_fd_context_t **handle, hos_fd_context_t *input);
hos_fd_context_t *find_context_by_fd(hos_fd_context_t *handle, size_t fd);
void delete_context_by_fd(hos_fd_context_t **handle, size_t fd);
void delete_all(hos_fd_context_t **handle);
#endif
