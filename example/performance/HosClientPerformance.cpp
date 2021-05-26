/*************************************************************************
    > File Name: HosClientPerformance.cpp
    > Author: pxz
    > Created Time: Sat 10 Oct 2020 05:26:02 PM CST
 ************************************************************************/
extern "C"
{
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<pthread.h>
#include<dirent.h>
#include<sys/stat.h>
#include<math.h>
#include<netinet/in.h>
#include<zlog.h>
}
#include"../../src/hos_client.h"
#include "MESA_handle_logger.h"

#define MAX_THREAD_NUM 32
#ifndef MIN
#define MIN(a,b) ((a) > (b)) ? (b) : (a)
#endif

#define STRING_SIZE 1024

typedef struct thread_info_s
{
    char object[STRING_SIZE];
    char bucket[STRING_SIZE];
    char file[100][STRING_SIZE];
    int mode;
    size_t  thread_num;
}thread_info_t;

struct timespec *g_finished;
char g_file_name[100][STRING_SIZE];
size_t g_mode;
size_t g_test_count;
size_t g_append_size;

static size_t  calc_time(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000 * 1000 * 1000 + end.tv_nsec - start.tv_nsec; 
}

int read_file_list(const char *path, char file_name[][STRING_SIZE])
{
    DIR *dir;
    struct dirent *ptr;
    int path_len = strlen(path);
    int file_num = 0;

    if ((dir=opendir(path)) == NULL)
    {
        perror("Open dir error...");
        exit(-1);
    }
    
    while (((ptr=readdir(dir)) != NULL) && (file_num < 100))
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if((ptr->d_type == DT_REG) || (ptr->d_type == DT_LNK))
        {
            memcpy(file_name[file_num], path, path_len);
            strcat(file_name[file_num], ptr->d_name);
        }
        else if(ptr->d_type == DT_DIR)    ///dir
        {
            continue;
        }
        file_num++;
    }
    closedir(dir);
    return 0;
}

static void callback(bool result, const char *error, const char *bucket, const char *object, void *userdata)
{
#if 0
    userdata_t *data = (userdata_t *)userdata;
    clock_gettime(CLOCK_MONOTONIC, data->finished);
#endif
    return ;
}

static int file_to_buffer(const char *file, char *buffer, size_t *len)
{
    FILE *fp = fopen(file, "r");
    int num = 0;
    *len = 0;
    if (fp == NULL)
    {
        printf("fopen file failed:%s\n", file);
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

static int upload_file(char *file, char *buff, int buff_len, thread_info_t *thread_info, char *performance_info)
{
    size_t i;
    FILE *fp = NULL;
    size_t fd[3000];
    struct timespec tstart, tend, twrite;
    long time_write = 0, time_upload = 0;
    size_t len = strlen(performance_info);
    char file_size[128];
    long record[1000] = {0};
    double variance = 0.00;
    double average = 0.00;
    long time = 0;
    //写文件
    
    //clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (i = 0; i < g_test_count; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        fp = fopen(file, "w+");
        if (fp == NULL)
        {
            printf("error:fopen failed\n");
            return -1;
        }
        if (fwrite(buff, buff_len, 1, fp) != 1)
        {
            printf("error:fwrite failed\n");
            fclose(fp);
            return -1;
        }
        fclose(fp);
        clock_gettime(CLOCK_MONOTONIC, &twrite);
        record[i] = calc_time(tstart, twrite);
        time_write += record[i];
    }
    //clock_gettime(CLOCK_MONOTONIC, &twrite);
    //time_write = calc_time(tstart, twrite);
    time_write /= g_test_count;

    //上传文件
    //clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (i = 0; i < g_test_count; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        fd[i] = hos_open_fd(thread_info->bucket, thread_info->object, callback, NULL, thread_info->thread_num, g_mode);
        if (hos_write(fd[i], file, 0, thread_info->thread_num) != HOS_CLIENT_OK)
        {
            printf("error:hos_write file:%s\n", file);
            return -1;
        }
        clock_gettime(CLOCK_MONOTONIC, &tend);
        time = calc_time(tstart, tend);
        time_upload += time;
        record[i] += time;
    }
    //clock_gettime(CLOCK_MONOTONIC, &tend);
    //time_upload = calc_time(tstart, tend);
    time_upload /= g_test_count;

    average = time_write + time_upload;
    for (i = 0; i < g_test_count; i++)
    {
        variance += pow((record[i] - average), 2);
    }
    variance /= g_test_count;

    sprintf(file_size, "%dk", buff_len / 1024);
    sprintf(&performance_info[len], "%-20lu%-20s%-20ld%-20ld%-20lf%-20lf\n",
            thread_info->thread_num, file_size, time_write, time_upload, average, sqrt(variance));
    
    return 0;
}

static int upload_buff(char * buff, int buff_len, thread_info_t *thread_info, char *performance_info)
{
    size_t i = 0;
    int j = 0;
    size_t fd[1000] = {0};
    size_t tmp = 0;
    size_t rest = 0;
    struct timespec tstart, ttmp;
    size_t len;
    char file_size[128];
    char append_size[128];
    size_t success_cnt = 0;
    int ret = 0;
    double variance = 0.00;
    double average = 0.00;
    long record[30000] = {0};

    if (g_mode & APPEND_MODE)
    {
        fd[0] = hos_open_fd(thread_info->bucket, thread_info->object, callback, NULL, thread_info->thread_num, g_mode);

        for (i = 0; i < g_test_count; i++)
        {
            clock_gettime(CLOCK_MONOTONIC, &tstart);
            j = 0;
            while (1)
            {
                tmp = j * g_append_size;
                rest = buff_len - tmp;
                if (rest < g_append_size)
                {
                    hos_write(fd[0], &buff[tmp], rest, thread_info->thread_num);
                    break;
                }
                hos_write(fd[0], &buff[tmp], g_append_size, thread_info->thread_num);
                j++;
            }
            clock_gettime(CLOCK_MONOTONIC, &ttmp);
            record[i] = calc_time(tstart, ttmp);
            average += record[i];
        }
        average /= g_test_count;

        for (i = 0; i < g_test_count; i++)
        {
            variance += pow((record[i] - average), 2);
        }
        variance /= g_test_count;

        if (buff_len > 1024 * 1024)
        {
            sprintf(file_size, "%gM", (double)buff_len / 1024 / 1024);
        }
        else if (buff_len > 1024)
        {
            sprintf(file_size, "%gK", (double)buff_len / 1024);
        }
        else
        {
            sprintf(file_size, "%dB", buff_len);
        }
        sprintf(append_size, "%gK", (double)g_append_size / 1024);
        len = strlen(performance_info);
        sprintf(&performance_info[len], "%-20lu%-20s%-20s%-20lu%-20lf%-20lf\n",
                thread_info->thread_num, file_size, append_size, g_test_count, average, sqrt(variance));
    }
    else
    {
        for (i = 0; i < g_test_count; i++)
        {
            clock_gettime(CLOCK_MONOTONIC, &tstart);
            fd[i] = hos_open_fd(thread_info->bucket, thread_info->object, callback, NULL, thread_info->thread_num, g_mode);
            ret = hos_write(fd[i], buff, buff_len, thread_info->thread_num);
            if (ret == HOS_CLIENT_OK)
            {
                success_cnt++;
            }
            else
            {
                //printf("error code:%d, thread_id:%d\n", ret, thread_info->thread_num);
                //break;
            }
            clock_gettime(CLOCK_MONOTONIC, &ttmp);
            record[i] = calc_time(tstart, ttmp);
            average += record[i];
        }
        if (success_cnt)
            average /= success_cnt;
        else
            average /= g_test_count;

        for (i = 0; i < g_test_count; i++)
        {
            variance += pow((record[i] - average), 2);
        }
        variance /= g_test_count;

        if (buff_len > 1024 * 1024)
        {
            sprintf(file_size, "%gM", (double)buff_len / 1024 / 1024);
        }
        else if (buff_len > 1024)
        {
            sprintf(file_size, "%gK", (double)buff_len / 1024);
        }
        else
        {
            sprintf(file_size, "%dB", buff_len);
        }
        sprintf(append_size, "%gK", (double)g_append_size / 1024);
        len = strlen(performance_info);
        sprintf(&performance_info[len], "%-20lu%-20s%-20d%-20lu%-20lf%-20lf\n",
                thread_info->thread_num, file_size, 0, g_test_count, average, sqrt(variance));
    }

    for (i = 0; i < g_test_count; i++)
    {
        if (fd[i] > 2)
        {
            hos_close_fd(fd[i], thread_info->thread_num);
        }
    }

    return 0;
}

static void *put_object_thread(void *ptr)
{
    char *performance_info = NULL;
    thread_info_t *thread_info = (thread_info_t *)ptr;
    char file[128];
    size_t buff_len;
    int ret;
    int i;
    char *buff = NULL;

    buff = (char *)malloc(30 * 1024 * 1024);
    if (buff == NULL)
    {
        perror(" ");
        pthread_exit(NULL);
    }

    performance_info = (char *)malloc(1024 * 1024);
    if (performance_info == NULL)
    {
        perror(" ");
        free(buff);
        pthread_exit(NULL);
    }
    memset(performance_info, 0, 10240);

    for (i = 0; i < 100; i++)
    {
        if (g_file_name[i][0] == '\0')
            break;
        ret = file_to_buffer(g_file_name[i], buff, &buff_len);
        if (ret == -1)
        {
            free(buff);
            free(performance_info);
            pthread_exit(NULL);
        }
        if (g_mode & BUFF_MODE)
        {
            upload_buff(buff, buff_len, thread_info, performance_info);
        }
        else
        {
            sprintf(file, "./file/file_%lu_%d", thread_info->thread_num, i);
            upload_file(file, buff, buff_len, thread_info, performance_info);
        }
    }
    free(buff);
    pthread_exit(performance_info);
}

int main(int argc, char *argv[])
{
    int ch;
    int buf_size;
    char *retval;
    size_t thread_num;
    size_t thread[MAX_THREAD_NUM];
    thread_info_t thread_info[MAX_THREAD_NUM];
    cpu_set_t mask;
    FILE *log = NULL;
    char log_name[STRING_SIZE];
    const char *log_prefix = "./";
    char conf_path[STRING_SIZE] = {0};
    char bucket[STRING_SIZE] = {0};
    char object[STRING_SIZE] = {0};
    char module[STRING_SIZE] = {0};
    char upload_file_path[STRING_SIZE] = {0};
    size_t thread_sum = 0;
    time_t timep;
    struct stat s_buf;

    /*init*/
    g_append_size = 102400;
    memcpy(bucket, "hos_test_bucket", strlen("hos_test_bucket"));
    memcpy(object, "object", strlen("object"));
    memcpy(conf_path, "../conf/default.conf", strlen("../conf/default.conf"));
    memcpy(module, "module", strlen("module"));
    memcpy(upload_file_path, "../CMakeLists.txt", strlen("../CMakeLists.txt"));
    thread_sum = 1;
    g_mode = BUFF_MODE;
    g_test_count = 100;

    //读取命令行配置
    while((ch = getopt(argc, argv, "a:b:c:o:m:f:t:M:n:h")) != -1)
    {
        switch(ch)
        {
            case 'a':
                g_append_size = atoi(optarg);
                break;
            case 'b':   /*bucket*/
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(bucket, optarg, buf_size);
                bucket[buf_size] = '\0';
                break;
            case 'c':   /*configuration file*/
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(conf_path, optarg, buf_size);
                conf_path[buf_size] = '\0';
                break;
            case 'o':   /*object*/
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(object, optarg, buf_size);
                object[buf_size] = '\0';
                break;
            case 'm':   /*module*/
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(module, optarg, buf_size);
                module[buf_size] = '\0';
                break;
            case 'f':   /*module*/
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(upload_file_path, optarg, buf_size);
                upload_file_path[buf_size] = '\0';
                break;
            case 't':
                thread_sum = atoi(optarg);
                break;
            case 'M':
                g_mode = atoi(optarg);
                break;
            case 'n':
                g_test_count = atoi(optarg);
                break;
            case 'h':
            default:
                printf("usage: HosClientPerformance \n"
                        "[-b set bucket] \n"
                        "[-c set conf file path] \n"
                        "[-o set object] \n"
                        "[-t set thread sum] \n"
                        "[-m set module] \n"
                        "[-f set upload file path] \n"
                        "[-M set mode] \n"
                        "[-h show help info] \n");
                return -1;
                break;
        }
    }

    strcpy(log_name, log_prefix);
    time(&timep);
    strftime(&log_name[strlen(log_prefix)], sizeof(log_name) - strlen(log_prefix),"%Y%m%d%H%M%S.log", localtime(&timep));
    log = fopen(log_name, "a+");
    if (log == NULL)
    {
        perror(log_name);
        return -1;
    }
    //初始化hos instance
    hos_instance hos_instance = hos_init_instance(conf_path, module, thread_sum, bucket);
    if (hos_instance == NULL)
    {
        printf("error:hos_client_handle\n %s\n", hos_instance->error_message);
        fclose(log);
        return -1;
    }

    //zlog_init("../conf/zlog.conf");
    MESA_handle_runtime_log_creation(NULL);
    printf("\n==============================================================================================================================\n");
    if (g_mode & BUFF_MODE)
    {
        printf("%-20s%-20s%-20s%-20s%-20s%-20s\n", "thread_id", "file_size", "append_size", "upload_time", "total_time", "std-dev");
    }else
    {
        printf("%-20s%-20s%-20s%-20s%-20s%-20s\n", "thread_id", "file_size", "write_time", "upload_time", "total_time", "std-dev");
    }

    memset(g_file_name, 0, 100 * 256);
    stat(upload_file_path, &s_buf);
    if (S_ISDIR(s_buf.st_mode))
    {
        read_file_list(upload_file_path, g_file_name);
        for (int i = 0; i < 100; i++)
        {
            if (g_file_name[i][0] == '\0')
                break;
        }
    }else
    {
        memcpy(g_file_name[0], upload_file_path, MIN(strlen(upload_file_path), STRING_SIZE - 1));
    }

    for ( thread_num = 0; thread_num < thread_sum; thread_num++ )
    {
        thread_info[thread_num].thread_num = thread_num;
        sprintf(thread_info[thread_num].object, "%s-%lu", object, thread_num);
        sprintf(thread_info[thread_num].bucket, "%s", bucket);

        if(pthread_create(&thread[thread_num], NULL, put_object_thread, (void *)&thread_info[thread_num]))
        {
            perror(" ");
            fclose(log);
            hos_shutdown_instance();
            return -1;
        }

        CPU_ZERO(&mask);
        CPU_SET(thread_num, &mask);
        if (pthread_setaffinity_np(thread[thread_num], sizeof(mask), &mask) != 0)
        {
            printf("warning:could not set CPU affinity, continuing...\n");
        }
    }

    for (thread_num = 0; thread_num < thread_sum; thread_num++)
    {
        pthread_join(thread[thread_num], (void **)&retval);
        if (retval)
        {
            printf("%s", retval);
            fwrite(retval, strlen(retval), 1, log);
            free(retval);
        }
    }

    if (hos_shutdown_instance() == 0)
    {
        //time = calc_time(start, finished);
        //time /= test_times;
        //printf("hos upload finished spent %llu ns\n", time);
    }

    fclose(log);
    return 0;
}
