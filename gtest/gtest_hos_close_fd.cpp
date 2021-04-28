#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"

TEST(hos_close_fd, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    size_t fd = hos_open_fd(HOS_BUCKET, "object", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_close_fd(fd, 0);
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

TEST(hos_close_fd, paramer_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int fd = hos_open_fd(NULL, "object", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, HOS_PARAMETER_ERROR);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_close_fd(fd, 0);
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

TEST(hos_close_fd, not_init_instance)
{
    int ret = hos_close_fd(1,1);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}

TEST(hos_close_fd, fd_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_close_fd(7, 0);
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