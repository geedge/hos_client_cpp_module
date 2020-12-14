/*************************************************************************
    > File Name: gtest_hos_create_bucket.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:32:14 AM CST
 ************************************************************************/
#include <gtest/gtest.h>
#include "hos_client.h"

TEST(hos_create_bucket, normal)
{
    hos_client_handle handle = hos_client_create("http://192.168.40.151/hos/", "default", "default", 1);
    int ret = hos_create_bucket(handle, "mybucket");
    EXPECT(HOS_CLIENT_OK, ret);
}

TEST(hos_create_bucket, parameter_error)
{
    hos_client_handle handle = hos_client_create("http://192.168.40.151/hos/", "default", "default", 1);
    int ret = hos_create_bucket(handle, NULL);
    EXPECT(HOS_CLIENT_OK, HOS_PARAMETER_ERROR);
}
