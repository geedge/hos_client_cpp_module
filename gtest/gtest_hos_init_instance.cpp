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
    instance->status = INSTANCE_ENABLE_STATE;
    instance->error_code = 0;
    instance->error_message[0] ='\0';
    instance->hos_url_prefix = "http://127.0.0.1:9098/hos/";
}

TEST(hos_init_instance, normal)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1);
    ASSERT_NE(hos_instance, nullptr);
    gtest_hos_instance_init(&expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    ASSERT_EQ(ret, HOS_CLIENT_OK);
    memset(&expect_hos_instance, 0, sizeof(expect_hos_instance));
    CheckHosInstance(hos_instance, &expect_hos_instance);

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
}

TEST(hos_init_instance, param_error)
{
    hos_instance_s expect_hos_instance;

    hos_instance hos_instance = hos_init_instance(NULL, "hos_default_conf", 1);
    ASSERT_EQ(hos_instance, nullptr);
    expect_hos_instance.status = INSTANCE_UNINIT_STATE;
    expect_hos_instance.error_code = HOS_PARAMETER_ERROR;
    const char *err_msg =  "param error:conf_path:NULL, module:hos_default_conf, thread_num:1";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1);
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(&g_hos_instance, &expect_hos_instance);
}

TEST(hos_init_instance, conf_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_conf", 1);
    ASSERT_EQ(hos_instance, nullptr);
    hos_instance_s expect_hos_instance;
    expect_hos_instance.status = INSTANCE_UNINIT_STATE;
    expect_hos_instance.error_code = HOS_CONF_ERROR;
    const char *err_msg =  "hos param error:hos ip:, hos port:0, accesskeyid:, secretkey:";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1);
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(&g_hos_instance, &expect_hos_instance);
}

TEST(hos_init_instance, server_conn_failed)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_server_conf", 1);
    memset(&expect_hos_instance, 0, sizeof(expect_hos_instance));
    expect_hos_instance.status = INSTANCE_ATTEMPT_STATE;
    expect_hos_instance.error_code = NETWORK_CONNECTION;
    const char *err_msg = "curlCode: 7, Couldn't connect to server";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1); 
    expect_hos_instance.hos_url_prefix = "http://192.168.40.146:9098/hos/";
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 1);
    memcpy(expect_hos_handle.hos_config.ip, "192.168.40.146", INET_ADDRSTRLEN);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    ASSERT_EQ(ret, HOS_CLIENT_OK);
    memset(&expect_hos_instance, 0, sizeof(expect_hos_instance));
    CheckHosInstance(hos_instance, &expect_hos_instance);

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
}