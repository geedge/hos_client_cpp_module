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

#define debuginfo (void)

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

void callback(bool result, const char *error, const char *bucket, const char *object, void *userdata)
{
    userdata_t *data = (userdata_t *)userdata;
    clock_gettime(CLOCK_MONOTONIC, data->finished);
    return ;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        debuginfo("usege: singThread [bucket name] [object name]\n");
        return -1;
    }
    struct timespec start, end, finished;
    size_t time;
    int i = 0;
    char *bucket = argv[1];
    char *object = argv[2];
    int test_times = atoi(argv[3]);
    //int test_times = 10000;
    //char *buf = (char *)malloc(1024 * 1024 * 4);
    char buf[1024 * 1024 * 4];
    //char buf[1024 * 4];
    size_t buf_size;
    int mode = FILE_MODE;
    size_t fd[10000] = {0}; 
    userdata_t data = {&finished};

    file_to_buffer(object, buf, &buf_size);

    debuginfo("hos_client_init start ...\n");
    hos_client_handle handle = hos_client_create("192.168.44.10", 9098, "default", "default", 3000);
    if (handle == NULL) 
    {
        debuginfo("error:hos_client_handle\n");
        return -1;
    }  
    debuginfo("hos_client_init success ... \n");

    debuginfo("hos_create_bucket start ... \n");
    if(hos_create_bucket(handle, bucket))
    {
        debuginfo("hos_create_bucket failed ... \n");
        return -1;
    }
    debuginfo("hos_create_bucket success ... \n");

    debuginfo("hos_verify_bucket start ... \n");
    if(!hos_verify_bucket(handle, bucket))
    {
        debuginfo("hos_verify_bucket failed ... \n");
        return -1;
    }
    debuginfo("hos_verify_bucket success ... \n");
    
#if 1
    debuginfo("hos_upload_file start ...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < test_times; i++)
    {
        hos_upload_file(handle, bucket, object, callback, (void *)&data, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    time = calc_time(start, end);
    time /= test_times;
    printf("hos_upload_file spent %llu ns\n", time);
    debuginfo("hos_upload_file end ...\n");
#else

    debuginfo("hos_upload_buf start ...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < test_times; i++)
    {
        hos_upload_buf(handle, bucket, object, buf, buf_len, callback, (void *)&data, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    time = calc_time(start, end);
    time /= test_times;
    printf("hos_upload_buf spent %llu ns\n", time);
    debuginfo("hos_upload_buf end ...\n");

#endif
    debuginfo("hos_client_close start ...\n");
    if (hos_client_destory(handle) == 0)
    {
        time = calc_time(start, finished);
        time /= test_times;
        printf("hos upload finished spent %llu ns\n", time);
    }

    debuginfo("hos_client_close end ...\n");

    return 0;
}
