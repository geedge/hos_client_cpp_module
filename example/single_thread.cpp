/*************************************************************************
    > File Name: single_thread.cpp
    > Author: pxz
    > Created Time: Fri 11 Sep 2020 09:52:05 AM CST
 ************************************************************************/
#include<stdio.h>
#include"../src/hos_client.h"

int main(int argc, char *argv[])
{
    hos_client_handle handle = hos_client_init("192.168.44.12:9098/hos", "default", "defalut");
    if (handle == NULL) 
    {
        printf("error:hos_client_handle\n");
        return -1;
    }  

    hos_create_bucket(handle, "my-first-bucket");

    hos_upload_async(handle, "my-first-bucket", "my-file.txt");
    
    hos_client_close(handle);

    return 0;
}
