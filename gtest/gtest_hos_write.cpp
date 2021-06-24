#include <aws/s3/model/Bucket.h>
#include "CheckHosClient.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "firewall_hos_bucket"
#define HOS_BUFF "This a googleTest"
#define HOS_FILE "../conf/default.conf"

static void gtest_hos_handle_init(hos_client_handle_t *hos_handle, int thread_num)
{
    memset(hos_handle, 0, sizeof(hos_client_handle_t));
    hos_handle->buckets.push_back(Aws::S3::Model::Bucket().WithName("session_record_hos_bucket"));
    hos_handle->buckets.push_back(Aws::S3::Model::Bucket().WithName("firewall_hos_bucket"));
    hos_handle->count = 1;
    memcpy(hos_handle->hos_config.accesskeyid, "default", strlen("default")+1);
    memcpy(hos_handle->hos_config.secretkey, "default", strlen("default")+1);
    hos_handle->hos_config.cache_count = 10;
    hos_handle->hos_config.cache_size = 102400;
    hos_handle->hos_config.fs2_fmt = 0;
    memcpy(hos_handle->hos_config.fs2_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    memcpy(hos_handle->hos_config.fs2_path, "./log/hos_fs2_log", strlen("./log/hos_fs2_log")+1);
    hos_handle->hos_config.fs2_port = 10086;
    memcpy(hos_handle->hos_config.ip, "127.0.0.1", strlen("127.0.0.1")+1);
    hos_handle->hos_config.log_level = 30;
    memcpy(hos_handle->hos_config.log_path, "./hoslog", strlen("./hoslog")+1);
    hos_handle->hos_config.pool_thread_size = 10;
    hos_handle->hos_config.port = 9098;
    hos_handle->hos_config.thread_num = thread_num;
    hos_handle->hos_config.max_request_num = 100;
    hos_handle->hos_config.max_request_context = 10240000;
    hos_handle->hos_func.fd_thread_status = 0;
    hos_handle->hos_func.fs2_status = 1;

    data_info_t *data_info = (data_info_t *)calloc(1, sizeof(data_info_t));
    hos_handle->hos_func.fs2_info[0].reserved = (void *)data_info;
    data_info->tx_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->rx_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->rx_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_failed_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_failed_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->cache = (size_t *)calloc(thread_num, sizeof(size_t));
}

static void gtest_hos_instance_init(hos_instance instance)
{
    memset(instance, 0, sizeof(hos_instance_s));
    instance->result = true;
    instance->error_code = 0;
    instance->error_message[0] ='\0';
    instance->hos_url_prefix = "http://127.0.0.1:9098/hos/";
}

static void gtest_hos_fd_init(hos_fd_context_t *fd_info)
{
    memset(fd_info, 0, sizeof(hos_fd_context_t));
    fd_info->bucket = (char *)HOS_BUCKET;
    fd_info->object = (char *)"object";
    fd_info->cache_count = 10;
    fd_info->cache_rest = g_hos_handle.hos_config.cache_size;
    fd_info->callback = NULL;
    fd_info->fd = 1;
    fd_info->fd_status = HOS_FD_REGISTER;
    fd_info->mode = BUFF_MODE;
    fd_info->position = 0;
    fd_info->recive_cnt = 0;
    fd_info->userdata = NULL;
}

static void hos_callback(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}

static void hos_write_buff_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}

static void hos_write_append_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}

static void hos_write_file_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}

static void hos_bucket_not_exits_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, false);
    EXPECT_STREQ(bucket, "bucket_not_exits");
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, "The specified bucket does not exist.");
}

TEST(hos_write, normal)
{
    int thread_num = 3;
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[3];
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    size_t fd = hos_open_fd(HOS_BUCKET, "object_buff", hos_write_buff_cb, (void *)"object_buff", 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[0]);
    expect_fd_info[0].callback = (void *)hos_write_buff_cb;
    expect_fd_info[0].object = (char *)"object_buff";
    expect_fd_info[0].userdata = (void *)"object_buff";
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);

    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_fd_info[0].fd_status = 2;
    data_info->rx_bytes[0] += strlen(HOS_BUFF);
    data_info->rx_pkts[0] +=1;
    data_info->tx_bytes[0] += strlen(HOS_BUFF);
    data_info->tx_pkts[0] += 1;

    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);

    size_t fd1 = hos_open_fd(HOS_BUCKET, "object_append", hos_write_append_cb, (void *)"object_append", 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[1]);
    expect_fd_info[1].callback = (void *)hos_write_append_cb;
    expect_fd_info[1].object = (char *)"object_append";
    expect_fd_info[1].userdata = (void *)"object_append";
    expect_fd_info[1].mode = BUFF_MODE | APPEND_MODE;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] +=1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    expect_fd_info[1].cache_count--;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] +=1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    expect_fd_info[1].cache_count--;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    size_t fd2 = hos_open_fd(HOS_BUCKET, "object_file", hos_write_file_cb, (void *)"object_file", 2, FILE_MODE);
    EXPECT_EQ(fd2, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[2]);
    expect_fd_info[2].callback = (void *)hos_write_file_cb;
    expect_fd_info[2].object = (char *)"object_file";
    expect_fd_info[2].userdata = (void *)"object_file";
    expect_fd_info[2].mode = FILE_MODE;
    CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);

    ret = hos_write(fd2, HOS_FILE, strlen(HOS_CONF), 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    struct stat buffer;
    stat(HOS_CONF, &buffer);
    data_info->rx_bytes[2] += buffer.st_size;
    data_info->rx_pkts[2] +=1;
    data_info->tx_bytes[2] += buffer.st_size;
    data_info->tx_pkts[2] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[2].fd_status = 2;
    //CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    //EXPECT_TRUE(g_fd_context[2][0].cache != NULL); 

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd1, 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->tx_bytes[1] += data_info->cache[1]; 
    data_info->tx_pkts[1] += 1;
    data_info->cache[1] = 0;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd2, 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, bucket_not_exits)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[3];
    int thread_num = 3;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    
    size_t fd = hos_open_fd("bucket_not_exits", "object_buff", hos_bucket_not_exits_cb, (void *)"object_buff", 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[0]);
    expect_fd_info[0].callback = (void *)hos_bucket_not_exits_cb;
    expect_fd_info[0].userdata = (void *)"object_buff";
    expect_fd_info[0].bucket = (char *)"bucket_not_exits";
    expect_fd_info[0].object = (char *)"object_buff";
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);
    
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[0] = strlen(HOS_BUFF);
    data_info->rx_pkts[0] += 1;
    data_info->tx_failed_bytes[0] = strlen(HOS_BUFF);
    data_info->tx_failed_pkts[0] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    size_t fd1 = hos_open_fd("bucket_not_exits", "object_append", hos_bucket_not_exits_cb, (void *)"object_append", 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[1]);
    expect_fd_info[1].callback = (void *) hos_bucket_not_exits_cb;
    expect_fd_info[1].userdata = (void *)"object_append";
    expect_fd_info[1].bucket = (char *)"bucket_not_exits";
    expect_fd_info[1].object = (char *)"object_append";
    expect_fd_info[1].mode = BUFF_MODE | APPEND_MODE;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->cache[1] += strlen(HOS_BUFF);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    expect_fd_info[1].cache_count--;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 
    
    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->cache[1] += strlen(HOS_BUFF);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    expect_fd_info[1].cache_count--;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    size_t fd2 = hos_open_fd("bucket_not_exits", "object_file", hos_bucket_not_exits_cb, (void *)"object_file", 2, FILE_MODE);
    EXPECT_EQ(fd2, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[2]);
    expect_fd_info[2].callback = (void *) hos_bucket_not_exits_cb;
    expect_fd_info[2].userdata = (void *)"object_file";
    expect_fd_info[2].bucket = (char *)"bucket_not_exits";
    expect_fd_info[2].object = (char *)"object_file";
    expect_fd_info[2].mode = FILE_MODE;
    CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);
    
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_CONF), 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_fd_info[2].fd_status = 2;
    struct stat buffer;
    stat(HOS_CONF, &buffer);
    data_info->rx_bytes[2] += buffer.st_size;
    data_info->rx_pkts[2] += 1;
    data_info->tx_failed_bytes[2] += buffer.st_size;
    data_info->tx_failed_pkts[2] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd1, 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->tx_failed_bytes[1] += data_info->cache[1];
    data_info->tx_failed_pkts[1] += 1;
    data_info->cache[1] = 0;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd2, 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, sync_mode)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[3];
    int thread_num = 3;
    data_info_t * data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_sync_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    expect_hos_handle.hos_config.pool_thread_size = 0;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    size_t fd = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[0]);
    expect_fd_info[0].object = (char *)"object_buff";
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);
    
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[0] += strlen(HOS_BUFF);
    data_info->rx_pkts[0] += 1;
    data_info->tx_bytes[0] += strlen(HOS_BUFF);
    data_info->tx_pkts[0] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);

    size_t fd1 = hos_open_fd(HOS_BUCKET, "object_append", NULL, NULL, 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[1]);
    expect_fd_info[1].object = (char *)"object_append";
    expect_fd_info[1].mode = BUFF_MODE | APPEND_MODE;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_count--;
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_count--;
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    size_t fd2 = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 2, FILE_MODE);
    EXPECT_EQ(fd2, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[2]);
    expect_fd_info[2].object = (char *)"object_buff";
    expect_fd_info[2].mode = FILE_MODE;
    CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_BUFF), 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    struct stat buffer;
    stat(HOS_CONF, &buffer);
    data_info->rx_bytes[2] += buffer.st_size;
    data_info->rx_pkts[2] += 1;
    data_info->tx_bytes[2] += buffer.st_size;
    data_info->tx_pkts[2] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[2].fd_status = 2;
    //CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd1, 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->tx_bytes[1] += data_info->cache[1];
    data_info->tx_pkts[1] += 1;
    data_info->cache[1] = 0;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd2, 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, sync_mode_bucket_not_exits)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[3];
    int thread_num = 3;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_sync_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    expect_hos_handle.hos_config.pool_thread_size = 0;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    
    size_t fd = hos_open_fd(HOS_CONF, "object_buff", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[0]);
    expect_fd_info[0].object = (char *)"object_buff";
    expect_fd_info[0].mode = BUFF_MODE;
    expect_fd_info[0].bucket = (char *)HOS_CONF;
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);

    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, NO_SUCH_BUCKET);
    expect_fd_info[0].fd_status = 2;
    data_info->rx_bytes[0] += strlen(HOS_BUFF);
    data_info->rx_pkts[0] += 1;
    data_info->tx_failed_bytes[0] += strlen(HOS_BUFF);
    data_info->tx_failed_pkts[0] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);

    size_t fd1 = hos_open_fd(HOS_CONF, "object_append", NULL, NULL, 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[1]);
    expect_fd_info[1].object = (char *)"object_append";
    expect_fd_info[1].mode = BUFF_MODE | APPEND_MODE;
    expect_fd_info[1].bucket = (char *)HOS_CONF;
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_count--;
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->rx_bytes[1] += strlen(HOS_BUFF);
    data_info->rx_pkts[1] += 1;
    data_info->cache[1] += strlen(HOS_BUFF);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    expect_fd_info[1].cache_count--;
    expect_fd_info[1].cache_rest -= strlen(HOS_BUFF);
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    EXPECT_TRUE(g_fd_context[1][0].cache != NULL); 

    size_t fd2 = hos_open_fd(HOS_CONF, "object_file", NULL, NULL, 2, FILE_MODE);
    EXPECT_EQ(fd2, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[2]);
    expect_fd_info[2].object = (char *)"object_file";
    expect_fd_info[2].mode = FILE_MODE;
    expect_fd_info[2].bucket = (char *)HOS_CONF;
    CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_FILE), 2);
    EXPECT_EQ(ret, NO_SUCH_BUCKET);
    struct stat buffer;
    stat(HOS_CONF, &buffer);
    data_info->rx_bytes[2] += buffer.st_size;
    data_info->rx_pkts[2] += 1;
    data_info->tx_failed_bytes[2] += buffer.st_size;
    data_info->tx_failed_pkts[2] += 1;
    expect_fd_info[2].fd_status = 2;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[2], &expect_fd_info[2]);

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd1, 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    data_info->tx_failed_bytes[1] += data_info->cache[1];
    data_info->tx_failed_pkts[1] += 1;
    data_info->cache[1] = 0;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    ret = hos_close_fd(fd2, 2);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, paramer_error)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    int thread_num = 2;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    
    int fd = hos_open_fd(HOS_BUCKET, "object_buff", hos_callback, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info);
    expect_fd_info.object = (char *)"object_buff";
    expect_fd_info.callback = (void *)hos_callback;
    expect_fd_info.mode = BUFF_MODE;
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);

    int ret = hos_write(fd, NULL, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, fd_not_find)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    int thread_num = 2;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_HASH_NOT_FIND);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    CheckStructGHosFdContext(g_fd_context[0], NULL);
    
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, file_not_exit)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    int thread_num = 2;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int fd = hos_open_fd(HOS_CONF, "object_file", NULL, NULL, 0, FILE_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info);
    expect_fd_info.bucket = (char *)HOS_CONF;
    expect_fd_info.object = (char *)"object_file";
    expect_fd_info.mode = FILE_MODE;
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);

    int ret = hos_write(fd, "not_exit_file", strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_FILE_NOT_EXIST);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, over_threadnums)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    int thread_num = 2;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int fd = hos_open_fd(HOS_BUCKET, "object", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info);
    expect_fd_info.object = (char *)"object";
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);
    
    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 6);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_write, not_init_instance)
{
    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}

static void *hos_function(void *ptr)
{
    #define HOS_FD_NUMS_LOCAL 20
    //int thread_id = (int)ptr;
    size_t thread_id = reinterpret_cast<size_t>(ptr);
    hos_instance hos_instance = NULL;
    int i = 0;
    int fd[HOS_FD_NUMS_LOCAL] = {0};
    char object[HOS_FD_NUMS_LOCAL][1024];
    int ret = 0;
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[32][20];
    int thread_num = 32;
    data_info_t *data_info = NULL;

    {
        hos_instance = hos_get_instance();
        if (hos_instance->result == false)
        {
            hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", thread_num, HOS_BUCKET);
        }
    }
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, thread_num);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info[0].reserved;
    expect_hos_handle.count = thread_id + 1;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    for (i = 0; i < HOS_FD_NUMS_LOCAL; i++)
    {
        snprintf(object[i], 1024, "object_%lu_%d", thread_id, i);
        fd[i] = hos_open_fd(HOS_BUCKET, object[i], hos_callback, object[i], 0, BUFF_MODE | APPEND_MODE);
        EXPECT_EQ(fd[i], i + 1);
        CheckHosInstance(hos_instance, &expect_hos_instance);
        CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
        gtest_hos_fd_init(&expect_fd_info[thread_id][i]);
        expect_fd_info[thread_id][i].object = object[i];
        expect_fd_info[thread_id][i].mode = BUFF_MODE | APPEND_MODE;
        expect_fd_info[thread_id][i].callback = (void *)hos_callback;
        CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[thread_id][i]);
    }

    for (i = 0; i < HOS_FD_NUMS_LOCAL; i++)
    {
        ret = hos_write(fd[i], HOS_BUFF, strlen(HOS_BUFF), i);
        EXPECT_EQ(ret, HOS_CLIENT_OK);
        data_info->cache[i] = strlen(HOS_BUFF);
        data_info->rx_bytes[i] = strlen(HOS_BUFF);
        data_info->rx_pkts[i] += 1; 
        CheckHosInstance(hos_instance, &expect_hos_instance);
        CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
        expect_fd_info[thread_id][i].cache_rest -= strlen(HOS_BUFF);
        expect_fd_info[thread_id][i].cache_count--;
        CheckStructGHosFdContext(&g_fd_context[thread_id][i], &expect_fd_info[thread_id][i]);
        EXPECT_TRUE(g_fd_context[1][0].cache != NULL);
    }

    for (i = 0; i < HOS_FD_NUMS_LOCAL; i++)
    {
        ret = hos_close_fd(fd[i], i);
        EXPECT_EQ(ret, 0);
        data_info->rx_bytes[i] = data_info->cache[i];
        data_info->rx_pkts[i] += 1; 
        data_info->cache[i] = 0;
        CheckHosInstance(hos_instance, &expect_hos_instance);
        CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    }

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, 0);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);

    pthread_exit(NULL);
#undef HOS_FD_NUMS_LOCAL
}

TEST(hos_write, mutil_thread)
{
    pthread_t thread_num[32];

    for (size_t i = 0; i < 32; i++)
    {
        pthread_create(&thread_num[i], NULL, hos_function, (void *)i);
    }
}