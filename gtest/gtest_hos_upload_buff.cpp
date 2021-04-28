#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"
#define HOS_BUFF "This is a googletest"

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
    EXPECT_STREQ(bucket, "bucket_not_exits");
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, "The specified bucket does not exist.");
}

TEST(hos_upload_buff, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_buf(HOS_BUCKET, "object", HOS_BUFF, strlen(HOS_BUFF), hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_upload_buff, bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_buf("bucket_not_exits", "object", HOS_BUFF, strlen(HOS_BUFF), hos_bucket_not_exits_cb, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_upload_buff, param_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_buf(NULL, "object", HOS_BUFF, strlen(HOS_BUFF), hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_upload_buff, not_init_instance)
{
    int ret  = hos_upload_buf(HOS_BUCKET, "object", HOS_BUFF, strlen(HOS_BUFF), hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}
