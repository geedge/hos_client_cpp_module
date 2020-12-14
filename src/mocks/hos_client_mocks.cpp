/*************************************************************************
    > File Name: hos_client_mocks.cpp
    > Author: pxz
    > Created Time: Wed 18 Nov 2020 01:57:18 PM CST
 ************************************************************************/
extern "C"
{
#include<string.h>
#include <sys/stat.h>
#include <unistd.h>
}
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/threading/Executor.h>
#include <fstream>
#include <iostream>
#include "hos_client.h"
#include "hos_hash.h"
#include "field_stat2.h"

Aws::S3::Model::ListBucketsOutcome MockListBuckets() const
{
    ListBucketsOutcome outcome;
    outcome.success = true;
}


