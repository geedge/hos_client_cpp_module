/*************************************************************************
    > File Name: gtest_hos_cleint_create.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:20:49 AM CST
 ************************************************************************/
#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"

TEST(hos_init_instance, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_init_instance, param_error)
{
    hos_instance hos_instance = hos_init_instance(NULL, "hos_default_conf", 1, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, false);
    EXPECT_EQ(hos_instance->error_code, HOS_PARAMETER_ERROR);
    EXPECT_STREQ(hos_instance->error_message, "param error:conf_path:(null), module:hos_default_conf, thread_num:1, bucket:hos_test_bucket");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_init_instance, no_fs2)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_no_fs2_conf", 1, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_init_instance, bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, "hos_bucket_not_exits");
    EXPECT_EQ(hos_instance->result, false);
    EXPECT_EQ(hos_instance->error_code, HOS_BUCKET_NOT_EXIST);
    EXPECT_STREQ(hos_instance->error_message, "bucket:hos_bucket_not_exits not exits.");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_init_instance, conf_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_conf", 1, "hos_bucket_not_exits");
    EXPECT_EQ(hos_instance->result, false);
    EXPECT_EQ(hos_instance->error_code, HOS_CONF_ERROR);
    EXPECT_STREQ(hos_instance->error_message, "hos param error:hos ip:, hos port:0, accesskeyid:, secretkey:");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_init_instance, server_conn_failed)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_error_server_conf", 1, "hos_bucket_not_exits");
    EXPECT_EQ(hos_instance->result, false);
    EXPECT_EQ(hos_instance->error_code,  NETWORK_CONNECTION);
    EXPECT_STREQ(hos_instance->error_message, "curlCode: 7, Couldn't connect to server");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}