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
#include<sys/stat.h>
}
#include"hos_client.h"

//#define test_times 10000

typedef struct userdata_s
{
    struct timespec *finished;
}userdata_t;

static size_t  calc_time(struct timespec start, struct timespec end)
{
    return (end.tv_sec * 1000 * 1000 * 1000 + end.tv_nsec - 
        (start.tv_sec * 1000 * 1000 * 1000 + start.tv_nsec));
}

int file_to_buffer(const char *file, char *buffer, int size)
{
    FILE *fp = fopen(file, "r");
    int num = 0;

    if (fp == NULL)
    {
        printf("fopen file failed:%s\n", file);
        return -1;
    }
    do{
        num = fread(buffer, 1, size, fp);
        if (num < 0)
        {
            return -1;
        }
        else if (num < size)
        {
            return 0;
        }
    }while(1);
    fclose(fp);
    return 0;
}

void callback(bool result, const char *bucket, const char *object, const char *errormsg, size_t errorcode, void *userdata)
{
    if (!result)
    {
        printf("error:[%d], %s\n", errorcode, errormsg);
    }
    return ;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("usege: [conf file] [module name] [file name]\n");
        return -1;
    }
    struct timespec start, end, finished;
    size_t time;
    int i = 0;
    char *conf_file = argv[1];
    char *module_name = argv[2];
    char *file_name = argv[3];
    struct stat buffer;    
    char *buf = NULL;
    size_t buf_size;
    int mode = FILE_MODE;
    size_t fd = 0; 
    userdata_t data = {&finished};
    hos_instance hos_instance = NULL;
    char object[1024];
    const char *bucket = "hos_test_bucket";

    if (stat(file_name, &buffer) == -1)
    {
        printf("%s not exits.", conf_file);
        return -1;
    }
    
    buf = (char *)calloc(1, buffer.st_size +1);

    if (file_to_buffer(file_name, buf, buffer.st_size) == -1)
    {
        free(buf);
        return -1;
    }

    printf("hos_init_instance start ...\n");
    hos_instance = hos_get_instance();
    if (hos_instance == NULL)
    {
        hos_instance = hos_init_instance(conf_file, module_name, 1);
    }
    if (hos_instance == NULL) 
    {
        printf("error:hos_init_instance\n");
        printf("error:[%d]%s", hos_get_init_instance_errorcode, hos_get_init_instance_errormsg);
        return -1;
    }  
    printf("hos_init_instance success ... \n");

    printf("hos_upload_file start ...\n");
    snprintf(object, 1023, "%s_upload_file", file_name);
    if (hos_upload_file(bucket, file_name, callback, NULL, 0) != HOS_CLIENT_OK)
    {
        printf("error: hos_upload_file fialed!\n");
    }
    printf("hos_upload_file end ...\n");

    printf("hos_upload_buff start ...\n");
    snprintf(object, 1023, "%s_upload_buff", file_name);
    if (hos_upload_buf(bucket, object, buf, buffer.st_size, callback, NULL, 0) != HOS_CLIENT_OK)
    {
        printf("error: hos_upload_buff failed!\n");
    }
    printf("hos_upload_buff end ...\n");

    mode = BUFF_MODE | APPEND_MODE;
    printf("hos_write buff start ...\n");
    snprintf(object, 1023, "%s_write_APPEND", file_name);
    fd = hos_open_fd(bucket, object, callback, NULL, 0);
    if (hos_write(fd, buf, buffer.st_size) != HOS_CLIENT_OK)
    {
        printf("error: hos_write failed 1st!\n");
    }
    if (hos_write(fd, buf, buffer.st_size) != HOS_CLIENT_OK)
    {
        printf("error: hos_write failed 2nd!\n");
    }
    hos_close_fd(fd);
    printf("hos_write buff end ...\n");

    printf("hos_shutdown_instance start ...\n");
    if (hos_shutdown_instance() != 0)
    {
        printf("error: hos_shutdown_instance fialed\n");
    }
    printf("hos_shutdown_instance end ...\n");
    
    free(buf);
    return 0;
}
