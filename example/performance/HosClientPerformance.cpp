extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <netinet/in.h>
}
#include "../../src/hos_client.h"
#include "../../src/hos_common.h"
#include "MESA_handle_logger.h"

char *g_uploadfile;
int g_loop;
int g_append_cnt;
extern hos_instance_s g_hos_instance;

static size_t  calc_time(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000 * 1000 * 1000 + end.tv_nsec - start.tv_nsec; 
}

static char *file_to_buffer(const char *file, size_t *len)
{
	FILE *fp = fopen(file, "r");
	if (fp == NULL)
	{
		printf("fopen file failed:%s\n", file);
		return NULL;
	}
	fseek(fp, 0L, SEEK_END);
	int filesize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	int num = 0;
	*len = 0;
	char *buffer = (char *)calloc(1, ((filesize / 4096)+1) * 4096);
	do
	{
		num = fread(&buffer[*len], 1, 4096, fp);
		if (num < 0)
		{
			fclose(fp);
			free(buffer);
			return NULL;
		}
		*len += num;
	} while (num == 4096);
	fclose(fp);
	return buffer;
}

struct put_finished_cb_ud
{
	struct timespec time_start;
	char *log;
};

static void put_finished_cb (bool result, const char *bucket, const char *object, const char *errmsg, size_t errorcode, void *userdata)
{
	struct timespec *time_finished = (struct timespec *)userdata;
	clock_gettime(CLOCK_MONOTONIC, time_finished);
	if (result == false)
	{
		printf("upload %s/%s failed. errmsg:%s errcode:%zu\n", bucket, object, errmsg, errorcode);
	}
}

static void *put_object_thread(void *ptr)
{
	size_t uploadfile_size = 0;
	char *uploadfile = file_to_buffer(g_uploadfile, &uploadfile_size);
	size_t thread_id = (size_t)ptr;
	size_t fd = 0;
	struct timespec time_start;
	struct timespec time_end;
	struct timespec time_finished;
	
	char *performance_info = (char *)calloc(1, 1024 * 1024);
	if (performance_info == NULL)
	{
		perror(" ");
		free(uploadfile);
		pthread_exit(NULL);
	}

	char object[1024];
	clock_gettime(CLOCK_MONOTONIC, &time_start);
	for (int i = 0; i < g_loop; i++)
	{
		sprintf(object, "object_%zu_%d", thread_id, i);
		hos_open_fd("hos_test_bucket", object, put_finished_cb, &time_finished, thread_id, &fd);
		for (int i = 0; i < g_append_cnt; i++)
		{
			hos_write(fd, uploadfile, uploadfile_size);
		}
		hos_close_fd(fd);
	}
	clock_gettime(CLOCK_MONOTONIC, &time_end);
	long write_successed = calc_time(time_start, time_end);

	free(uploadfile);
	while(g_hos_instance.status != INSTANCE_UNINIT_STATE)
	{
		sleep(1);
	}
	long upload_successed = calc_time(time_start, time_finished);

	snprintf(performance_info, 1024 * 1024, "%-15zu%-15zu%-15d%-15d%-15ld%-15ld%-25ld%-25ld",
		thread_id, uploadfile_size, g_loop, g_append_cnt,
		write_successed/g_loop, upload_successed/g_loop,
		write_successed/g_loop/g_append_cnt, upload_successed/g_loop/g_append_cnt);

	pthread_exit(performance_info);
}

int main(int argc, char *argv[])
{
	// if (argc != )
	if (argc != 8)
	{
		printf("usage: HosClientPerformance <conf> <module> <thread_num> <log> <uploadfile> <loop(not less than 1)> <append_cnt(not less than 1)>.\n");
		return -1;
	}

	char *conf = argv[1];
	char *module = argv[2];
	int thread_num = atoi(argv[3]);
	char *log = argv[4];
	g_uploadfile = argv[5];
	g_loop = atoi(argv[6]);
	g_append_cnt = atoi(argv[7]);

	if (g_loop < 1 or g_append_cnt < 1)
	{
		printf("usage: HosClientPerformance <conf> <module> <thread_num> <log> <uploadfile> <loop(not less than 1)> <append_cnt(not less than 1)>.\n");
		return -1;
	}
	cpu_set_t mask;

	MESA_handle_runtime_log_creation(log);
	printf("\n======================================================================================================================================================\n");
	printf("%-15s%-15s%-15s%-15s%-15s%-15s%-25s%-25s\n", "thread_id", "file_size", "loop", "append_cnt", "write/loop", "upload/loop", "write/loop/append_cnt", "upload/loop/append_cnt");
	//初始化hos instance
	hos_instance hos_instance = hos_init_instance(conf, module, thread_num);
	if (hos_instance == NULL)
	{
		printf("error:hos_client_handle\nerror:[%d]%s\n", hos_get_init_instance_errorcode(), hos_get_init_instance_errormsg());
		MESA_handle_runtime_log_destruction();
		return -1;
	}

	pthread_t *thread_id = (pthread_t *)calloc(thread_num, sizeof(pthread_t));

	for (int i = 0; i < thread_num; i++)
	{
		pthread_create(&thread_id[i], NULL, put_object_thread, (void *)(long)i);

		CPU_ZERO(&mask);
		CPU_SET(thread_num, &mask);
		if (pthread_setaffinity_np(thread_id[i], sizeof(mask), &mask) != 0)
		{
			printf("warning:could not set CPU affinity, continuing...\n");
		}
	}

	hos_shutdown_instance();

	char *retval = NULL;
	for (int i = 0; i < thread_num; i++)
	{
		pthread_join(thread_id[i], (void **)&retval);
		if (retval)
		{
			printf("%s\n", retval);
			// fwrite(retval, strlen(retval), 1, log);
			free(retval);
		}
	}
	free(thread_id);

	return 0;
}