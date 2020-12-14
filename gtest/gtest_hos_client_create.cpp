/*************************************************************************
    > File Name: gtest_hos_cleint_create.cpp
    > Author: pxz
    > Created Time: Tue 29 Sep 2020 10:20:49 AM CST
 ************************************************************************/
#include <gtest/gtest.h>
#include "hos_client.h"

TEST(hos_client_create, normal)
{
    hos_client_handle handle = NULL;
    handle = hos_client_create("http://192.168.40.151/hos/", "default", "default", 1);
    EXPECT_NE((void *)NULL, handle);
}

TEST(hos_client_create, parameter_error)
{
    hos_client_handle handle = NULL;
    handle = hos_client_create(NULL, "default", "default", 1);
    EXPECT_EQ((void *)NULL, handle);
}
