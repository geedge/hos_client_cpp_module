/*************************************************************************
    > File Name: gtest_hos_cleint_create.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:20:49 AM CST
 ************************************************************************/
#include <aws/s3/model/Bucket.h>
#include "CheckHosClient.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "firewall_hos_bucket"

TEST(hos_init_instance, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = true;
    expect_hos_instance.error_code = 0;
    expect_hos_instance.error_message[0] ='\0';
    expect_hos_instance.hos_url_prefix = "http://127.0.0.1:9098/hos/";
    CheckHosInstance(hos_instance, &expect_hos_instance);

    hos_client_handle_t expect_hos_handle;
    memset(&expect_hos_handle, 0, sizeof(expect_hos_handle));
    expect_hos_handle.buckets.push_back(Aws::S3::Model::Bucket().WithName("session_record_hos_bucket"));
    expect_hos_handle.buckets.push_back(Aws::S3::Model::Bucket().WithName("firewall_hos_bucket"));
    expect_hos_handle.count = 1;
    memcpy(expect_hos_handle.hos_config.accesskeyid, "default", strlen("default")+1);
    memcpy(expect_hos_handle.hos_config.secretkey, "default", strlen("default")+1);
    expect_hos_handle.hos_config.cache_count = 10;
    expect_hos_handle.hos_config.cache_size = 102400;
    expect_hos_handle.hos_config.fs2_fmt = 0;
    memcpy(expect_hos_handle.hos_config.fs2_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    memcpy(expect_hos_handle.hos_config.fs2_path, "./log/hos_fs2_log", strlen("./log/hos_fs2_log")+1);
    expect_hos_handle.hos_config.fs2_port = 10086;
    memcpy(expect_hos_handle.hos_config.ip, "127.0.0.1", strlen("127.0.0.1")+1);
    expect_hos_handle.hos_config.log_level = 30;
    memcpy(expect_hos_handle.hos_config.log_path, "./hoslog", strlen("./hoslog")+1);
    expect_hos_handle.hos_config.pool_thread_size = 10;
    expect_hos_handle.hos_config.port = 9098;
    expect_hos_handle.hos_config.thread_num = 1;
    expect_hos_handle.hos_config.timeout = 1000;
    expect_hos_handle.hos_func.fd_thread_status = 0;
    expect_hos_handle.hos_func.fs2_status = 1;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.error_code = 0;
    expect_hos_instance.error_message[0] ='\0';
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
}

TEST(hos_init_instance, param_error)
{
    hos_instance hos_instance = hos_init_instance(NULL, "hos_default_conf", 1, HOS_BUCKET);

    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = false;
    expect_hos_instance.error_code = HOS_PARAMETER_ERROR;
    const char *err_msg =  "param error:conf_path:(null), module:hos_default_conf, thread_num:1, bucket:firewall_hos_bucket";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1);
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);
}

TEST(hos_init_instance, no_fs2)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_no_fs2_conf", 1, HOS_BUCKET);
    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = true;
    expect_hos_instance.error_code = 0;
    expect_hos_instance.error_message[0] ='\0';
    expect_hos_instance.hos_url_prefix = "http://127.0.0.1:9098/hos/";
    CheckHosInstance(hos_instance, &expect_hos_instance);

    hos_client_handle_t expect_hos_handle;
    memset(&expect_hos_handle, 0, sizeof(expect_hos_handle));
    expect_hos_handle.buckets.push_back(Aws::S3::Model::Bucket().WithName("session_record_hos_bucket"));
    expect_hos_handle.buckets.push_back(Aws::S3::Model::Bucket().WithName("firewall_hos_bucket"));
    expect_hos_handle.count = 1;
    memcpy(expect_hos_handle.hos_config.accesskeyid, "default", strlen("default")+1);
    memcpy(expect_hos_handle.hos_config.secretkey, "default", strlen("default")+1);
    expect_hos_handle.hos_config.cache_count = 10;
    expect_hos_handle.hos_config.cache_size = 102400;
    expect_hos_handle.hos_config.fs2_fmt = 0;
    expect_hos_handle.hos_config.fs2_ip[0] = '\0';
    expect_hos_handle.hos_config.fs2_path[0] = '\0';
    //memcpy(expect_hos_handle.hos_config.fs2_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    memcpy(expect_hos_handle.hos_config.fs2_path, "./log/hos_fs2_log", strlen("./log/hos_fs2_log")+1);
    //expect_hos_handle.hos_config.fs2_port = 10086;
    expect_hos_handle.hos_config.fs2_port = 0;
    memcpy(expect_hos_handle.hos_config.ip, "127.0.0.1", strlen("127.0.0.1")+1);
    expect_hos_handle.hos_config.log_level = 30;
    memcpy(expect_hos_handle.hos_config.log_path, "./hoslog", strlen("./hoslog")+1);
    expect_hos_handle.hos_config.pool_thread_size = 10;
    expect_hos_handle.hos_config.port = 9098;
    expect_hos_handle.hos_config.thread_num = 1;
    expect_hos_handle.hos_config.timeout = 1000;
    expect_hos_handle.hos_func.fd_thread_status = 0;
    expect_hos_handle.hos_func.fs2_status = 0;
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.result = 0;
    expect_hos_instance.error_code = 0;
    expect_hos_instance.error_message[0] ='\0';
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
}

TEST(hos_init_instance, bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, "hos_bucket_not_exits");

    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = false;
    expect_hos_instance.error_code = HOS_BUCKET_NOT_EXIST;
    const char *err_msg =  "bucket:hos_bucket_not_exits not exits.";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1);
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    hos_client_handle_t expect_hos_handle;
    memset(&expect_hos_handle, 0, sizeof(expect_hos_handle));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
}

TEST(hos_init_instance, conf_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_conf", 1, "hos_bucket_not_exits");
    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = false;
    expect_hos_instance.error_code = HOS_CONF_ERROR;
    const char *err_msg =  "hos param error:hos ip:, hos port:0, accesskeyid:, secretkey:";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1);
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);
}

TEST(hos_init_instance, server_conn_failed)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_server_conf", 1, "hos_bucket_not_exits");
    hos_instance_s expect_hos_instance;
    expect_hos_instance.result = false;
    expect_hos_instance.error_code = NETWORK_CONNECTION;
    const char *err_msg = "curlCode: 7, Couldn't connect to server";
    memcpy(expect_hos_instance.error_message, err_msg, strlen(err_msg)+1); 
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);
}