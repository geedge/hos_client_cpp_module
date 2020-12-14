/*************************************************************************
    > File Name: gtest_hos_verify_bucket.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:32:14 AM CST
 ************************************************************************/
#include <gtest/gtest.h>
#include "hos_client.h"

TEST(hos_verify_bucket, normal)
{
    int ret = -1;
    hos_client_handle handle = hos_client_create("http://192.168.40.151/hos/", "default", "default", 1);
    if (hos_create_bucket(handle, "mybcuket"))
        ret = hos_verify_bucket(handle, "mybucket");
    EXPECT(HOS_CLIENT_OK, ret);
}

