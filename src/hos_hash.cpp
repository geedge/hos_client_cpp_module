/*************************************************************************
    > File Name: uthash.cpp
    > Author: pxz
    > Created Time: Fri 18 Sep 2020 04:26:09 PM CST
 ************************************************************************/
#include "hos_hash.h"

void add_fd_context(hos_fd_context_t **handle, hos_fd_context_t *input)
{
    hos_fd_context_t *value = NULL;
    HASH_FIND_INT(*handle, (int *)&input->fd, value);
    if (value == NULL)
    {
        value = (hos_fd_context_t *)malloc(sizeof(hos_fd_context_t));
        memcpy(value, input, sizeof(hos_fd_context_t));
        value->object = (char *)calloc(1, strlen(input->object) + 1);
        value->bucket = (char *)calloc(1, strlen(input->bucket) + 1);
        memcpy(value->bucket, input->bucket, strlen(input->bucket));
        memcpy(value->object, input->object, strlen(input->object));
        HASH_ADD_INT(*handle, fd, value);
    }
    else
    {
        value->mode = input->mode;
        if (value->object != NULL)
        {
            free(value->object);
            value->object = NULL;
        }
        if (value->bucket != NULL)
        {
            free(value->bucket);
            value->bucket = NULL;
        }
        value->object = (char *)calloc(1, strlen(input->object) + 1);
        value->bucket = (char *)calloc(1, strlen(input->bucket) + 1);
        memcpy(value->bucket, input->bucket, strlen(input->bucket));
        memcpy(value->object, input->object, strlen(input->object));
        value->callback = input->callback;
        value->userdata = input->userdata;
        value->cache = input->cache;
        value->cache_count = input->cache_count;
        value->cache_rest = input->cache_rest;
        value->position = input->position;
        value->recive_cnt = input->recive_cnt;
        value->fd_status = value->fd_status;
    }
}

hos_fd_context_t *find_context_by_fd(hos_fd_context_t *handle, size_t fd)
{
    hos_fd_context_t *value = NULL;
    HASH_FIND_INT(handle, &fd, value);
    return value;
}

void delete_context_by_fd(hos_fd_context_t **handle, size_t fd)
{
    hos_fd_context_t *value = NULL;

    HASH_FIND_INT(*handle, &fd, value);
    if (value)
    {
        if (value->bucket)
        {
            free(value->bucket);
            value->bucket = NULL;
        }
        if (value->object)
        {
            free(value->object);
            value->object = NULL;
        }
        HASH_DEL(*handle, value);
        free(value);
    }
}

void delete_all(hos_fd_context_t **handle)
{
    hos_fd_context_t *current, *tmp;
    HASH_ITER(hh, *handle, current, tmp)
    {
        if (current->bucket)
        {
            free(current->bucket);
            current->bucket = NULL;
        }
        if (current->object)
        {
            free(current->object);
            current->object = NULL;
        }
        HASH_DEL(*handle, current);
        free(current);
    }
}
