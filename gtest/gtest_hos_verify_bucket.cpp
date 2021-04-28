/*************************************************************************
    > File Name: gtest_hos_verify_bucket.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:32:14 AM CST
 ************************************************************************/
#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"

TEST(hos_verify_bucket, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    bool result = hos_verify_bucket(HOS_BUCKET);
    EXPECT_EQ(result, true);
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

TEST(hos_verify_bucket, not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 1, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    bool result = hos_verify_bucket("hos_not_exits_bucket");
    EXPECT_EQ(result, false);
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

TEST(hos_verify_bucket, no_instance)
{
    bool result = hos_verify_bucket("hos_not_exits_bucket");
    EXPECT_EQ(result, false);
}
