/*************************************************************************
    > File Name: hos_client_api.cpp
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:00:23 PM CST
 ************************************************************************/
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include "hos_client.h"

std::mutex upload_mutex;

static void PutObjectAsyncFinished(const Aws::S3::S3Client* s3Client, 
    const Aws::S3::Model::PutObjectRequest& request, 
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    if (outcome.IsSuccess()) {
        std::cout << "Success: PutObjectAsyncFinished: Finished uploading '" 
            << context->GetUUID() << "'." << std::endl;
    }
    else {
        std::cout << "Error: PutObjectAsyncFinished: " <<
            outcome.GetError() << std::endl;
    }

}

hos_client_handle hos_client_init(const char *endpoint, const char *accesskeyid, const char *secretkey)
{
    if (!endpoint || !accesskeyid || !secretkey)
    {
        return NULL;
    }
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    hos_client_handle handle = NULL;
    Aws::Client::ClientConfiguration config;
    Aws::Auth::AWSCredentials credentials(accesskeyid, secretkey);
    //std::cout << "accesskeyid: " << credentials.GetAWSAccessKeyId() << "\n" << std::endl;
    //std::cout << "secretkey: " << credentials.GetAWSSecretKey() << "\n" << std::endl;

    config.endpointOverride = endpoint;
    config.verifySSL = false;
    config.enableEndpointDiscovery = true;

    handle = new Aws::S3::S3Client(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    return handle;
}

int hos_create_bucket(hos_client_handle handle, const char *bucket)
{
    if (!bucket)
    {
        return -1;
    }
    Aws::S3::S3Client& s3Client = *(Aws::S3::S3Client *) handle;
    Aws::S3::Model::CreateBucketRequest createBucketRequest;
    createBucketRequest.SetBucket(bucket);
    //std::cout << "bucket name: " << createBucketRequest.GetBucket() << "\n" << std::endl;

    Aws::S3::Model::CreateBucketOutcome createBucketOutcome = s3Client.CreateBucket(createBucketRequest);

    if (!createBucketOutcome.IsSuccess())
    {
        Aws::S3::S3Errors errorcode = createBucketOutcome.GetError().GetErrorType();
        if (errorcode != Aws::S3::S3Errors::BUCKET_ALREADY_OWNED_BY_YOU)
        {
            //std::cout << "Failed to create bucket: " << bucket << "\n" << createBucketOutcome.GetError() << std::endl;
            return errorcode;
        }
    }

    return 0;
}

bool hos_upload_async(hos_client_handle handle, const char *bucket, const char *object)
{
    Aws::S3::S3Client& s3Client = *(Aws::S3::S3Client *) handle;
    struct stat buffer;

    std::unique_lock<std::mutex> lock(upload_mutex);
    if (stat(object, &buffer) == -1)
    {
        //error: file does not exist.
        return false;
    }

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(object);

    const std::shared_ptr<Aws::IOStream> input_data =
        Aws::MakeShared<Aws::FStream>("SampleAllocationTag", object, std::ios_base::in | std::ios_base::binary);

    request.SetBody(input_data);

    std::shared_ptr<Aws::Client::AsyncCallerContext> context =
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("PutObjectAllocationTag");
    context->SetUUID(object);
    s3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    return true;
}

void hos_client_close(hos_client_handle handle)
{
    if (handle == NULL)
    {
        return;
    }

    delete (Aws::S3::S3Client *)handle;

    return ;
}
