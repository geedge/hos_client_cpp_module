/*************************************************************************
    > File Name: uthash.cpp
    > Author: pxz
    > Created Time: Fri 18 Sep 2020 04:26:09 PM CST
 ************************************************************************/
#include "hos_hash.h"

void add_hos_info(hos_info_t **handle, hos_info_t *input)
{
    hos_info_t *value = NULL;
    HASH_FIND_INT(*handle, &input->fd, value);
    if (value == NULL)
    {
        value = (hos_info_t *)malloc(sizeof(hos_info_t));
        memcpy(value, input, sizeof(hos_info_t));
        HASH_ADD_INT(*handle, fd, value);
    }
    else
    {
        value->bucket = input->bucket;
        value->object = input->object;
        value->callback = input->callback;
        value->userdata = input->userdata;
    }
}

hos_info_t *find_info_by_fd(hos_info_t *handle, size_t fd)
{
    hos_info_t *value = NULL;
    HASH_FIND_INT(handle, &fd, value);
    return value;
}

void delete_info_by_fd(hos_info_t *handle, size_t fd)
{
    hos_info_t *value = NULL;
    HASH_FIND_INT(handle, &fd, value);
    if (value)
    {
        HASH_DEL(handle, value);
        free(value);
    }
}

void delete_all(hos_info_t *handle)
{
    hos_info_t *current, *tmp;
    HASH_ITER(hh, handle, current, tmp)
    {
        HASH_DEL(handle, current);
    }
}
