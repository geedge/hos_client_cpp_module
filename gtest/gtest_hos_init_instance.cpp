/*************************************************************************
    > File Name: gtest_hos_cleint_create.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:20:49 AM CST
 ************************************************************************/
#include <aws/external/gtest.h>
#include "hos_client.h"

TEST(hos_init_instance, normal)
{
    hos_instance hos_instance = hos_init_instance("../conf/default.conf", "hos_default_conf", 1, "hos_test_bucket");
    EXPECT_EQ(hos_instance->result, true);
    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
}

TEST(hos_open_fd, buffer)
{
    hos_instance hos_instance = hos_init_instance("../conf/default.conf", "hos_default_conf", 1, "hos_test_bucket");
    EXPECT_EQ(hos_instance->result, true);
    int fd = hos_open_fd("hos_test_bucket", "hos_test_object", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
}