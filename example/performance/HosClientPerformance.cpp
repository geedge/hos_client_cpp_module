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
}
#include"../../src/hos_client.h"

#define MAX_THREAD_NUM 32
#ifndef MIN
#define MIN(a,b) ((a) > (b)) ? (b) : (a)
#endif

typedef struct conf_s
{
#define STRING_SIZE 128
    char endpoint[STRING_SIZE];
    char bucket[STRING_SIZE];
    char object[STRING_SIZE];
    char file[STRING_SIZE];
    size_t pool_size;
    size_t thread_sum;
    size_t size;
    float append_size;
    size_t slice;
    int mode;
}conf_t;

typedef struct thread_info_s
{
    conf_t conf;
    hos_client_handle handle;
    size_t  thread_num;
}thread_info_t;

static void configuration_init(conf_t *conf)
{
    strcpy(conf->endpoint, "http://192.168.40.223:9098/hos/");
    strcpy(conf->bucket, "mybucket");
    strcpy(conf->object, "myobject");
    strcpy(conf->file, "./file/test.txt");
    conf->pool_size = 4000;
    conf->append_size = 1024;
    conf->thread_sum = 1;
    conf->mode = BUFF_MODE;
    conf->slice = 0;

    return ;
}

typedef struct userdata_s
{
    struct timespec *finished;
}userdata_t;

static size_t  calc_time(struct timespec start, struct timespec end)
{
    return (end.tv_sec * 1000 * 1000 * 1000 + end.tv_nsec - 
        (start.tv_sec * 1000 * 1000 * 1000 + start.tv_nsec));
}

int read_file_list(const char *path, char file_name[][256])
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
        else if((ptr->d_type == 8) || (ptr->d_type == 10))
        {
            memcpy(file_name[file_num], path, path_len);
            strcat(file_name[file_num], ptr->d_name);
        }
        else if(ptr->d_type == 4)    ///dir
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

static int upload_file(char *file, char *buff, int buff_len, int test_times, thread_info_t *thread_info, char *performance_info)
{
    int i;
    FILE *fp = NULL;
    size_t fd[3000];
    conf_t *conf = &thread_info->conf;
    struct timespec tstart, tend, twrite;
    long time_write = 0, time_upload = 0;
    size_t len = strlen(performance_info);
    char file_size[128];
    long record[1000] = {0};
    double variance = 0.00;
    double average = 0.00;
    long time = 0;
    int success_cnt;
    //写文件
    
    //clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (i = 0; i < test_times; i++)
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
    time_write /= test_times;

    //上传文件
    //clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (i = 0; i < test_times; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        fd[i] = hos_open_fd(thread_info->handle, conf->bucket, conf->object, callback, NULL, thread_info->thread_num, conf->mode);
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
    time_upload /= test_times;

    average = time_write + time_upload;
    for (i = 0; i < test_times; i++)
    {
        variance += pow((record[i] - average), 2);
    }
    variance /= test_times;

    sprintf(file_size, "%dk", buff_len / 1024);
    sprintf(&performance_info[len], "%-20lu%-20s%-20ld%-20ld%-20lf%-20lf\n",
            thread_info->thread_num, file_size, time_write, time_upload, average, sqrt(variance));
    
    return 0;
}

static int upload_buff(char * buff, int buff_len, int test_times, thread_info_t *thread_info, char *performance_info)
{
    int i = 0;
    int j = 0;
    size_t fd = 0;
    size_t tmp = 0;
    size_t rest = 0;
    struct timespec tstart, ttmp;
    size_t time = 0;
    size_t len;
    conf_t *conf = &thread_info->conf;
    char file_size[128];
    char append_size[128];
    size_t success_cnt = 0;
    int ret = 0;
    int drop = 0;
    double variance = 0.00;
    double average = 0.00;
    long record[1000] = {0};

    if (conf->slice)
    {
        fd = hos_open_fd(thread_info->handle, conf->bucket, conf->object, callback, NULL, thread_info->thread_num, conf->mode);
        for (i = 0; i < test_times; i++)
        {
            j = 0;
            clock_gettime(CLOCK_MONOTONIC, &tstart);
            while(1)
            {
                tmp = j * conf->slice;
                rest = buff_len - tmp;
                if (rest < conf->slice)
                {
                    hos_write(fd, &buff[tmp], rest, thread_info->thread_num);
                    break;
                }
                hos_write(fd, &buff[tmp], conf->slice, thread_info->thread_num);
                j++;
            }
            clock_gettime(CLOCK_MONOTONIC, &ttmp);
            record[i] = calc_time(tstart, ttmp);
            average += record[i];
        }
        average /= test_times;

        for (i = 0; i < test_times; i++)
        {
            variance += pow((record[i] - average), 2);
        }
        variance /= test_times;

        sprintf(file_size, "%dk", buff_len / 1024);
        sprintf(append_size, "%gk", conf->append_size / 1024);
        len = strlen(performance_info);
        sprintf(&performance_info[len], "%-20lu%-20s%-20s%-20d%-20lf%-20lf\n",
                thread_info->thread_num, file_size, append_size, 0, average, sqrt(variance));
    }else{

        if (conf->mode & APPEND_MODE)
        {
            fd = hos_open_fd(thread_info->handle, conf->bucket, conf->object, callback, NULL, thread_info->thread_num, conf->mode);
#if 1
            for (i = 0; i < test_times; i++)
            {
                clock_gettime(CLOCK_MONOTONIC, &tstart);
#endif
                j = 0;
                while(1)
                {
                    tmp = j * conf->append_size;
                    rest = buff_len - tmp;
                    if (rest < conf->append_size)
                    {
                        hos_write(fd, &buff[tmp], rest, thread_info->thread_num);
                        break;
                    }
                    hos_write(fd, &buff[tmp], conf->append_size, thread_info->thread_num);
                    j++;
                }
#if 1
                clock_gettime(CLOCK_MONOTONIC, &ttmp);
                record[i] = calc_time(tstart, ttmp);
                average += record[i];
            }
            average /= test_times;
#endif

            for (i = 0; i < test_times; i++)
            {
                variance += pow((record[i] - average), 2);
            }
            variance /= test_times;

            sprintf(file_size, "%dk", buff_len / 1024);
            sprintf(append_size, "%gk", conf->append_size / 1024);
            len = strlen(performance_info);
            sprintf(&performance_info[len], "%-20lu%-20s%-20s%-20d%-20lf%-20lf\n",
                    thread_info->thread_num, file_size, append_size, 0, average,sqrt(variance));
        }else
        {
            for (i = 0; i < test_times; i++)
            {
                clock_gettime(CLOCK_MONOTONIC, &tstart);
                fd = hos_open_fd(thread_info->handle, conf->bucket, conf->object, callback, NULL, thread_info->thread_num, conf->mode);
                ret = hos_write(fd, buff, buff_len, thread_info->thread_num);
                if (ret == HOS_CLIENT_OK)
                {
                    success_cnt++;
                }else
                {
                    printf("error code:%d, thread_id:%d\n", ret, thread_info->thread_num);
                    //break;
                }
                clock_gettime(CLOCK_MONOTONIC, &ttmp);
                record[i] = calc_time(tstart, ttmp);
                average += record[i];
            }
            if (success_cnt)
                average /= success_cnt;
            else
                average /= test_times;

            for (i = 0; i < test_times; i++)
            {
                variance += pow((record[i] - average), 2);
            }
            variance /= test_times;

            sprintf(file_size, "%dk", buff_len / 1024);
            sprintf(append_size, "%luk", conf->append_size / 1024);
            len = strlen(performance_info);
            sprintf(&performance_info[len], "%-20lu%-20s%-20d%-20d%-20lf%-20lf\n",
                    thread_info->thread_num, file_size, 0, 0, average, sqrt(variance));
        }
    }

    hos_close_fd(fd, thread_info->thread_num);

    return 0;
}

static void *put_object_thread(void *ptr)
{
    char *performance_info = NULL;
    thread_info_t *thread_info = (thread_info_t *)ptr;
    conf_t *conf = &thread_info->conf;
    char file[128];
    size_t buff_len;
    int ret;
    int i;
    char *buff = NULL;
    char file_name[100][256];
    struct stat s_buf;
    int test_times = 100;

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
    memset(file_name, 0, 100 *256);

    stat(conf->file, &s_buf);
    if (S_ISDIR(s_buf.st_mode))
    {
        read_file_list(conf->file, file_name);
        for (i = 0; i < 100; i++)
        {
            if (file_name[i][0] == '\0')
                break;
            ret = file_to_buffer(file_name[i], buff, &buff_len);
            if (ret == -1)
            {
                free(buff);
                free(performance_info);
                pthread_exit(NULL);
            }
            if (conf->mode & BUFF_MODE)
            {
                upload_buff(buff, buff_len, test_times, thread_info, performance_info);
            }else
            {
                sprintf(file, "./file/file_%lu_%d", thread_info->thread_num, i);
                upload_file(file, buff, buff_len, test_times, thread_info, performance_info);
            }
        }
    }else
    {
        ret = file_to_buffer(conf->file, buff, &buff_len);
        if (ret == -1)
        {
            free(buff);
            free(performance_info);
            pthread_exit(NULL);
        }

        if (conf->mode & BUFF_MODE)
        {
            upload_buff(buff, buff_len, test_times, thread_info, performance_info);
        }else
        {
            sprintf(file, "./file/file_%lu", thread_info->thread_num);
            upload_file(file, buff, buff_len, test_times, thread_info, performance_info);
        }
    }
    free(buff);
    pthread_exit(performance_info);
}

int main(int argc, char *argv[])
{
    int ch;
    int buf_size;
    conf_t conf;
    char *object;
    char *retval;
    size_t thread_num;
    size_t thread[MAX_THREAD_NUM];
    thread_info_t thread_info[MAX_THREAD_NUM];
    cpu_set_t mask;
    FILE *log = NULL;
    char log_name[256];
    const char *log_prefix = "./log/";
    time_t timep;
#if 0
    if (argc <= 1)
    {
        printf("usage: HosClientPerformance \n[-e set endpoint] \n[-b set bucket] \n"
                "[-o set object] \n[-f set file] \n[-p set pool size] \n"
                "[-t set thread sum] \n[-B set BUFF_MODE] \n"
                "[-F set FILE_MODE] \n[-A set APPEND_MODE] \n"
                "[-h show help info] \n");
        return -1;
    }
#endif
    configuration_init(&conf);
    //读取命令行配置
    while((ch = getopt(argc, argv, "a:e:b:o:f:p:t:k:s:S:BFAh")) != -1)
    {
        switch(ch)
        {
            case 'a':
                conf.append_size = 1024 * atof(optarg);
                break;
            case 'e':
                //endpoint
                buf_size = MIN(STRING_SIZE,  strlen(optarg));
                strncpy((char *)conf.endpoint, optarg, buf_size);
                break; 
            case 'b':
                buf_size = MIN(STRING_SIZE,  strlen(optarg));
                strncpy((char *)conf.bucket, optarg, buf_size);
                conf.bucket[buf_size] = '\0';
                break;          
            case 'o':
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(conf.object, optarg, buf_size);
                conf.object[buf_size] = '\0';
                break;
            case 'f':
                buf_size = MIN(STRING_SIZE, strlen(optarg));
                strncpy(conf.file, optarg, buf_size);
                conf.file[buf_size] = '\0';
                break;
            case 'p':
                conf.pool_size = atoi(optarg);
                conf.pool_size = MIN(4000, conf.pool_size);
                break;
            case 't':
                conf.thread_sum = atoi(optarg);
                break;
            case 'B':
                conf.mode &= BUFF_MODE;
                break;
            case 'F':
                conf.mode &= FILE_MODE;
                break;
            case 'A':
                conf.mode |= APPEND_MODE;
                break;
            case 'k':
                conf.append_size = 1024 * atoi(optarg);
                break;
            case 's':
                conf.size = atoi(optarg);
                break;
            case 'S':
                conf.slice = atoi(optarg);
                break;
            case 'h':
            default:
                printf("usage: HosClientPerformance \n[-e set endpoint] \n[-b set bucket] \n"
                        "[-o set object] \n[-f set file] \n[-p set pool size] \n"
                        "[-t set thread sum] \n[-B set BUFF_MODE] \n"
                        "[-F set FILE_MODE] \n[-A set APPEND_MODE] \n"
                        "[-h show help info] \n[-a set append size(K)]\n");
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
    //创建client
    hos_client_handle handle = hos_client_create(conf.endpoint, "default", "default", conf.pool_size);
    if (handle == NULL)
    {
        printf("error:hos_client_handle\n");
        fclose(log);
        return -1;
    }

    hos_set_thread_sum(handle, conf.thread_sum);
    hos_set_cache_size(handle, conf.append_size);
    hos_set_cache_times(handle, 0);

    //创建bucket
    if (hos_create_bucket(handle, conf.bucket))
    {
        printf("error:hos_create_bucket\n");
        fclose(log);
        hos_client_destory(handle);
        return -1;
    }

    printf("\n==============================================================================================================================\n");
    if (conf.mode & BUFF_MODE)
    {
        printf("%-20s%-20s%-20s%-20s%-20s\n", "thread_id", "file_size", "append_size", "upload_time", "total_time");
    }else
    {
        printf("%-20s%-20s%-20s%-20s%-20s\n", "thread_id", "file_size", "write_time", "upload_time", "total_time");
    }
    hos_expand_fs2(handle, "./log/fs2.log", 0, "127.0.0.1", 8001);
    for ( thread_num = 0; thread_num < conf.thread_sum; thread_num++ )
    {
        thread_info[thread_num].conf = conf;
        object = thread_info[thread_num].conf.object;
        sprintf(&object[strlen(object)], "-%lu", thread_num);
        thread_info[thread_num].thread_num = thread_num;
        thread_info[thread_num].handle = handle;
        if(pthread_create(&thread[thread_num], NULL, put_object_thread, (void *)&thread_info[thread_num]))
        {
            perror(" ");
            fclose(log);
            hos_client_destory(handle);
            return -1;
        }

        CPU_ZERO(&mask);
        CPU_SET(thread_num, &mask);
        if (pthread_setaffinity_np(thread[thread_num], sizeof(mask), &mask) != 0)
        {
            printf("warning:could not set CPU affinity, continuing...\n");
        }
    }

    for (thread_num = 0; thread_num < conf.thread_sum; thread_num++)
    {
        pthread_join(thread[thread_num], (void **)&retval);
        if (retval)
        {
            printf("%s", retval);
            fwrite(retval, strlen(retval), 1, log);
            free(retval);
        }
    }

    if (hos_client_destory(handle) == 0)
    {
        //time = calc_time(start, finished);
        //time /= test_times;
        //printf("hos upload finished spent %llu ns\n", time);
    }

    fclose(log);
    return 0;
}
