/*************************************************************************
    > File Name: single_thread.cpp
    > Author: pxz
    > Created Time: Fri 11 Sep 2020 09:52:05 AM CST
 ************************************************************************/
extern "C"
{
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
}
#include"hos_client.h"

//#define test_times 10000

#define debuginfo printf

typedef struct userdata_s
{
    struct timespec *finished;
}userdata_t;

static size_t  calc_time(struct timespec start, struct timespec end)
{
    return (end.tv_sec * 1000 * 1000 * 1000 + end.tv_nsec - 
        (start.tv_sec * 1000 * 1000 * 1000 + start.tv_nsec));
}

int file_to_buffer(const char *file, char *buffer, size_t *len)
{
    FILE *fp = fopen(file, "r");
    int num = 0;
    *len = 0;
    if (fp == NULL)
    {
        debuginfo("fopen file failed:%s\n", file);
        return -1;
    }
    do{
        num = fread(&buffer[*len], 1, 4096, fp);
        if (num < 0)
        {
            return -1;
        }
        *len += num;
    }while(num == 4096);
    fclose(fp);
    return 0;
}

void callback(bool result, const char *error, void *userdata)
{
    return ;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        debuginfo("usege: singThread [bucket name] [object name]\n");
        return -1;
    }
    int i = 0;
    char *bucket = argv[1];
    char *object = argv[2];
    int test_times = atoi(argv[3]);
    //int test_times = 10000;
    char *buf = (char *)malloc(1024 * 1024 * 40);
    //char buf[1024 * 1024 * 4];
    //char buf[1024 * 4];
    size_t buf_size;
    int mode = FILE_MODE;
    size_t fd[10001] = {0}; 
    hos_client_handle *handle = NULL;
    //printf("finished:%lu\n", finished.tv_sec * 1000 * 1000 * 1000 + finished.tv_nsec);

    file_to_buffer(object, buf, &buf_size);

    debuginfo("hos_client_init start ...\n");
    handle = (hos_client_handle *)calloc(5, sizeof(hos_client_handle));
    hos_init_api();
    for (i = 0; i < test_times; i++)
    {
        handle[i] = hos_client_create("http://192.168.40.223:9098/hos/", "default", "default", 400);
        if (handle[i] == NULL) 
        {
            printf("error:hos_client_handle\n");
            return -1;
        }  
        debuginfo("hos_client_init success ... \n");

        debuginfo("hos_create_bucket start ... \n");
        if(hos_create_bucket(handle[i], bucket))
        {
            printf("hos_create_bucket failed ... \n");
            return -1;
        }
        debuginfo("hos_create_bucket success ... \n");

        debuginfo("hos_verify_bucket start ... \n");
        if(!hos_verify_bucket(handle[i], bucket))
        {
            printf("hos_verify_bucket failed ... \n");
            return -1;
        }
        debuginfo("hos_verify_bucket success ... \n");

        mode = FILE_MODE;
        debuginfo("hos_upload_file start ...\n");
        fd[i] = hos_open_fd(handle[i], bucket, object, callback, NULL, mode);
        hos_write(fd[i], object, 0);
        debuginfo("hos_client_close start ...\n");
    }
    for (i = 0; i < test_times; i++)
    {
        hos_client_destory(handle[i]);
        debuginfo("hos_client_close end ...\n");
    }
    hos_shutdown_api();
    free(handle);
    return 0;
}
