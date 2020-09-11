/*************************************************************************
    > File Name: hos_client_api.h
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:13:59 PM CST
 ************************************************************************/
#ifndef __HOS_CLIENT_INIT__
#define __HOS_CLIENT_INIT__

typedef void* hos_client_handle;

hos_client_handle hos_client_init(const char *endpoint, const char *accesskeyid, const char *secretkey);
bool hos_create_bucket(hos_client_handle handle, const char *bucket);
bool hos_upload_async(hos_client_handle handle, const char *bucket, const char *object);
void hos_client_close(hos_client_handle handle);
#endif
