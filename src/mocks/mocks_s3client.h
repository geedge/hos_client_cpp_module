/*************************************************************************
    > File Name: mocks_s3client.h
    > Author: pxz
    > Created Time: Wed 18 Nov 2020 02:21:13 PM CST
 ************************************************************************/
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
namespace Aws
{
    namespace S3
    {
        class AWS_S3_API MocksS3client : public Aws::S3::S3Client
        {
            public:
                MocksListBuckets()
                {
#ifndef MOCKS
                    ListBuckets();
#else
                    
#endif
                }
        }

    }
}
