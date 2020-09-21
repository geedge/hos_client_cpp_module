/*************************************************************************
    > File Name: hos_hash.h
    > Author: pxz
    > Created Time: Fri 18 Sep 2020 05:00:04 PM CST
 ************************************************************************/
#ifndef __HOS_HASH_H__
#define __HOS_HASH_H__

#include "uthash.h"

typedef struct hos_info_s
{
    size_t fd;
    int  mode;
    void *handle; 
    const char *bucket;
    const char *object;
    void *callback;
    void *userdata;
    UT_hash_handle hh;
}hos_info_t;

void add_hos_info(hos_info_t **handle, hos_info_t *input);
hos_info_t *find_info_by_fd(hos_info_t *handle, size_t fd);
void delete_info_by_fd(hos_info_t *handle, size_t fd);
void delete_all(hos_info_t *handle);
#endif
