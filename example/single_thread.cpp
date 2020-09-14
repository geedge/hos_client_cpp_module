/*************************************************************************
    > File Name: single_thread.cpp
    > Author: pxz
    > Created Time: Fri 11 Sep 2020 09:52:05 AM CST
 ************************************************************************/
#include<stdio.h>
#include<unistd.h>
#include"../src/hos_client.h"

int main(int argc, char *argv[])
{
    char *bucket = argv[1];
    printf("hos_client_init start ...\n");
    hos_client_handle handle = hos_client_init("http://192.168.44.12:9098/hos", "default", "default");
    if (handle == NULL) 
    {
        printf("error:hos_client_handle\n");
        return -1;
    }  
    printf("hos_client_init success ...");

    printf("hos_create_bucket start ... \n");
    if(!hos_create_bucket(handle, bucket))
    {
        printf("hos_create_bucket failed ... \n");
    }
    printf("hos_create_bucket success ... \n");

    printf("hos_upload_async start ...\n");
    hos_upload_async(handle, bucket, "my-file.txt");
    
    //sleep(30);
    hos_client_close(handle);

    return 0;
}
