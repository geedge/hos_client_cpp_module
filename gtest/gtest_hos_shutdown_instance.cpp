/*************************************************************************
    > File Name: gtest_hos_cleint_create.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:20:49 AM CST
 ************************************************************************/
#include <aws/s3/model/Bucket.h>
#include "CheckHosClient.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "firewall_hos_bucket"

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
    instance->result = true;
    instance->error_code = 0;
    instance->error_message[0] ='\0';
    instance->hos_url_prefix = "http://127.0.0.1:9098/hos/";
}


TEST(hos_shutdown_instance, normal)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 1);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
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

TEST(hos_shutdown_instance, no_init)
{
    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}

TEST(hos_shutdown_instance, shutdown_more)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 1);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
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

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}
