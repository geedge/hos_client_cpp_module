#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"
#define HOS_BUFF "Makefile"

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
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, "The specified bucket does not exist.");
}

TEST(hos_upload_file, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_file(HOS_BUCKET, HOS_BUFF, hos_callback, (void *)HOS_BUFF, 0);
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

TEST(hos_upload_file, param_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_file(NULL, "object", hos_callback, (void *)"object", 0);
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

TEST(hos_upload_file, file_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_file(HOS_BUCKET, "file_not_exits", hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_FILE_NOT_EXIST);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_upload_file, not_init_instance)
{
    int ret  = hos_upload_file(HOS_BUCKET, HOS_BUFF, hos_callback, (void *)"object", 0);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}

TEST(hos_upload_file, bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret  = hos_upload_file(HOS_BUCKET, HOS_CONF, hos_bucket_not_exits_cb, (void *)HOS_CONF, 0);
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
