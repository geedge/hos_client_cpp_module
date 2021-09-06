#include <aws/s3/model/Bucket.h>
#include "CheckHosClient.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "firewall_hos_bucket"
#define HOS_BUFF "Makefile"

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
    hos_handle->hos_func.fs2_info.reserved = (void *)data_info;
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
    instance->status = INSTANCE_ENABLE_STATE;
    instance->error_code = 0;
    instance->error_message[0] ='\0';
    instance->hos_url_prefix = "http://127.0.0.1:9098/hos/";
}

static void hos_callback(bool result, const char *bucket, const char *object, const char *error, void *userdata)
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
    EXPECT_STREQ(bucket, HOS_CONF);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, "The specified bucket does not exist.");
}

TEST(hos_upload_file, normal)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret  = hos_upload_file(HOS_BUCKET, HOS_BUFF, hos_callback, (void *)HOS_BUFF, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    struct stat buffer;
    stat(HOS_BUFF, &buffer);
    data_info->rx_bytes[0] += buffer.st_size;
    data_info->rx_pkts[0] += 1;
    data_info->tx_bytes[0] += buffer.st_size;
    data_info->tx_pkts[0] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
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
}

TEST(hos_upload_file, param_error)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret  = hos_upload_file(NULL, "object", hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
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
}

TEST(hos_upload_file, file_not_exits)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret  = hos_upload_file(HOS_BUCKET, "file_not_exits", hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_FILE_NOT_EXIST);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
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
}

TEST(hos_upload_file, not_init_instance)
{
    int ret  = hos_upload_file(HOS_BUCKET, HOS_BUFF, hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_ENABLE);
}

TEST(hos_upload_file, bucket_not_exits)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;
    data_info_t *data_info = NULL;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret  = hos_upload_file(HOS_CONF, HOS_CONF, hos_bucket_not_exits_cb, (void *)HOS_CONF, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    struct stat buffer;
    stat(HOS_CONF, &buffer);
    data_info->rx_bytes[0] += buffer.st_size;
    data_info->rx_pkts[0] += 1;
    data_info->tx_failed_bytes[0] += buffer.st_size;
    data_info->tx_failed_pkts[0] += 1;
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
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
}
