/*************************************************************************
    > File Name: hos_client_api.cpp
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:00:23 PM CST
 ************************************************************************/
extern "C"
{
#include<string.h>
}
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/utils/threading/Executor.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include "hos_client.h"
#include "hos_hash.h"

#define MAX_HOS_CLIENT_THREAD_NUM 255
#define MAX_HOS_CLIENT_FD_NUM 65535

typedef struct hos_client_handle_s
{
    Aws::S3::S3Client *S3Client;
    Aws::SDKOptions options;
    Aws::Vector<Aws::S3::Model::Bucket> buckets;
    /* options */
    size_t cache_size;
    size_t cache_times;
    size_t thread_sum;
}hos_client_handle_t;

hos_info_t *hash_hos_info[MAX_HOS_CLIENT_THREAD_NUM];
size_t fd_info[MAX_HOS_CLIENT_THREAD_NUM][MAX_HOS_CLIENT_FD_NUM];

static size_t hash_get_min_free_fd(size_t thread_id)
{
    size_t i = 0;
    for (i = 1; i < MAX_HOS_CLIENT_FD_NUM; i++)
    {
        if (!fd_info[thread_id][i])
            return i; 
    }
    return 0;
}

static void PutObjectAsyncFinished(const Aws::S3::S3Client* S3Client, 
    const Aws::S3::Model::PutObjectRequest& request, 
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    const char *error = NULL;
    hos_info_t *hos_info = NULL;
    bool result = outcome.IsSuccess();
    if (!result)
    {
        error = outcome.GetError().GetMessage().c_str();
    }
    const Aws::String& uuid = context->GetUUID();
    size_t thread_id, fd;
    sscanf(uuid.c_str(), "%lu %lu", &thread_id, &fd);
    if (fd_info[thread_id][fd])
    {
        hos_info = find_info_by_fd(hash_hos_info[thread_id], fd);
    }
    if (hos_info == NULL)
    {
        return ; 
    }
    put_finished_callback callback = (put_finished_callback)hos_info->callback;
    callback(result, error, hos_info->userdata);
    if (hos_info->mode & APPEND_MODE)
    {
        //APPEND MODE 保留fd
    }else
    {
        //完整上传 删除fd
        hos_close_fd(fd, thread_id);
    }
}

void set_cache_size(hos_client_handle client,  size_t cache_size)
{
    client->cache_size = cache_size;
    return ;
}

void set_cache_times(hos_client_handle client, size_t cache_times)
{
    client->cache_times = cache_times;
    return ;
}

void set_thread_sum(hos_client_handle client, size_t thread_sum)
{
    client->thread_sum = thread_sum;
    return ;
}

hos_client_handle hos_client_create(const char *endpoint, const char *accesskeyid, const char *secretkey, size_t pool_size)
{
    if (!endpoint || !accesskeyid || !secretkey)
    {
        return NULL;
    }

    Aws::SDKOptions options;
    //options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
    Aws::InitAPI(options);

    hos_client_handle handle = (hos_client_handle)malloc(sizeof(hos_client_handle_t));
    memset(handle, 0, sizeof(hos_client_handle_t));
    Aws::Client::ClientConfiguration config;
    Aws::Auth::AWSCredentials credentials(accesskeyid, secretkey);

    //初始化
    config.endpointOverride = endpoint;
    config.verifySSL = false;
    config.enableEndpointDiscovery = true;
    config.executor = std::shared_ptr<Aws::Utils::Threading::PooledThreadExecutor>(std::make_shared<Aws::Utils::Threading::PooledThreadExecutor>(pool_size));//支持线程池

    handle->S3Client = new Aws::S3::S3Client(credentials, config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    handle->options = options;
    /* 获取当前用户的所有的buckets */
    Aws::S3::Model::ListBucketsOutcome outcome = handle->S3Client->ListBuckets();

    if (outcome.IsSuccess())
    {
        handle->buckets = outcome.GetResult().GetBuckets();
    }

    handle->cache_size = 0;
    handle->cache_times = 1;
    handle->thread_sum = 1;

    return handle;
}

bool hos_verify_bucket(hos_client_handle handle, const char *bucket)
{
    Aws::S3::Model::ListBucketsOutcome outcome = handle->S3Client->ListBuckets();

    if (outcome.IsSuccess())
    {
        handle->buckets = outcome.GetResult().GetBuckets();

        for (Aws::S3::Model::Bucket& new_bucket : handle->buckets)
        {
            if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

int hos_create_bucket(hos_client_handle handle, const char *bucket)
{
    if ((bucket == NULL) || (handle == NULL))
    {
        return HOS_PARAMETER_ERROR;
    }
    Aws::S3::S3Client& S3Client = *handle->S3Client;

    /* 本地检查是否已经存在该bucket */
    for (Aws::S3::Model::Bucket& new_bucket : handle->buckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
            return HOS_CLIENT_OK;
        }
    }

    Aws::S3::Model::CreateBucketRequest createBucketRequest;
    createBucketRequest.SetBucket(bucket);

    Aws::S3::Model::CreateBucketOutcome createBucketOutcome = S3Client.CreateBucket(createBucketRequest);

    if (!createBucketOutcome.IsSuccess())
    {
        Aws::S3::S3Errors errorcode = createBucketOutcome.GetError().GetErrorType();
        if (errorcode != Aws::S3::S3Errors::BUCKET_ALREADY_OWNED_BY_YOU)
        {
            return (int)errorcode + 1;
        }
    }

    return HOS_CLIENT_OK;
}

static int hos_upload_stream(hos_client_handle handle, const char *bucket, const char *object, 
        const char *data, size_t data_len, put_finished_callback callback, void *userdata, size_t thread_id, int file_type)
{
    struct stat buffer;
    char buf[128];
    size_t fd = hash_get_min_free_fd(thread_id);

    if ((handle == NULL) || (bucket == NULL) || (object == NULL) || (callback == NULL) || (thread_id > handle->thread_sum))
    {
        return HOS_PARAMETER_ERROR;
    }
    Aws::S3::S3Client& S3Client = *handle->S3Client;

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(object);
    
    //设置上传数据类型
    if (file_type == 0)
    {
        if (stat(data, &buffer) == -1)
        {
            return HOS_FILE_NOT_EXITS;
        }
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag", object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
    }
    else
    {
        //内存块
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::StringStream>(data);
        Aws::String stream (data, data_len);
        *input_data << stream;
        request.SetBody(input_data);
    }

    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu", thread_id, fd);
    context->SetUUID(buf);
    
    hos_info_t info = {fd, 0, handle, bucket, object, (void *)callback, userdata, NULL, 0, 0, 0 };
    add_hos_info(&hash_hos_info[thread_id], &info);
    fd_info[thread_id][fd] = 1;
    
    S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    return HOS_CLIENT_OK;
}

int hos_upload_file(hos_client_handle handle, const char *bucket, const char *file_path, 
        put_finished_callback callback, void *userdata, size_t thread_id)
{
    return hos_upload_stream(handle, bucket, file_path, NULL, 0, callback, userdata, thread_id, 0);
}

int hos_upload_buf(hos_client_handle handle, const char *bucket, const char *object, 
        const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id)
{
    return hos_upload_stream(handle, bucket, object, buf, buf_len, callback, userdata, thread_id, 1);
}

int hos_open_fd(hos_client_handle handle, const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, int mode)
{
    if ((handle == NULL) || (bucket == NULL) || (object == NULL) || (thread_id > handle->thread_sum))
    {
        return HOS_PARAMETER_ERROR;
    }

    size_t fd = hash_get_min_free_fd(thread_id);
    if (fd == 0)
    {
        return HOS_FD_NOT_ENOUGH;
    }
    
    hos_info_t info = {fd, mode, handle, bucket, object, (void *)callback, userdata, NULL, handle->cache_size, handle->cache_times, 0, };
    add_hos_info(&hash_hos_info[thread_id], &info);
    fd_info[thread_id][fd] = 1;

    return fd;
}

int hos_write(size_t fd, const char *stream, size_t stream_len, size_t thread_id)
{
    struct stat buffer;
    hos_info_t *hos_info = NULL;
    hos_client_handle handle = NULL;
    char num[128];
    char buf[128];
    int flag = 0; // 0, 一次处理就可以完成；1，需要多次处理才能处理完
    int rest;   // stream 剩余未处理的数据长度
    if ((fd == 0) || (stream == NULL) || (thread_id > MAX_HOS_CLIENT_THREAD_NUM))
    {
        return HOS_PARAMETER_ERROR;
    }

    if (fd_info[thread_id][fd])
    {
        hos_info = find_info_by_fd(hash_hos_info[thread_id], fd);
    }
    if (hos_info == NULL)
    {
        return HOS_HASH_NOT_FIND;
    }

    handle = (hos_client_handle)hos_info->handle;
    Aws::S3::S3Client& S3Client = *(handle->S3Client);

    // Create and configure the asynchronous put object request.
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(hos_info->bucket);
    request.SetKey(hos_info->object);

    //设置上传数据类型
    if (hos_info->mode & BUFF_MODE)
    {
        //BUFF_MODE
        if (hos_info->mode & APPEND_MODE)
        {
            //APPEND_MODE
            if (hos_info->cache == NULL)
            {
                hos_info->cache = Aws::MakeShared<Aws::StringStream>("append mode");
            }
            if ((--hos_info->cache_times) && (stream_len <= hos_info->cache_rest))
            {
                // cache
                Aws::String buffer (stream, stream_len);
                hos_info->cache_rest -= stream_len;
                if (hos_info->cache_rest > 0)
                {
                    return HOS_CLIENT_OK;
                }
            }else if (stream_len > hos_info->cache_rest)
            {
                // multi handle
                flag = 1;
                Aws::String buffer (stream, hos_info->cache_rest);
                *hos_info->cache << buffer;
                rest = stream_len - hos_info->cache_rest;
            }else
            {
                //over cache_times
                Aws::String buffer (stream, stream_len);
                *hos_info->cache << buffer;
            }
            request.SetBody(hos_info->cache);

            // add headers
            snprintf(num, 128, "%lu", ++hos_info->position);
            Aws::Map<Aws::String, Aws::String> headers;
            if (hos_info->mode & APPEND_MODE)
            {
                headers["x-hos-upload-type"] = "append";
                headers["x-hos-position"] = num;
                request.SetMetadata(headers);
            }
        }else
        {
            const std::shared_ptr<Aws::IOStream> input_data = 
                Aws::MakeShared<Aws::StringStream>("buffer mode");
            request.SetBody(input_data);
        }
    }
    else
    {
        if (stat(stream, &buffer) == -1)
        {
            return HOS_FILE_NOT_EXITS;
        }
        //文件类型
        const std::shared_ptr<Aws::IOStream> input_data = 
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag", hos_info->object, std::ios_base::in | std::ios_base::binary);
        request.SetBody(input_data);
    }

    //设置回调函数
    std::shared_ptr<Aws::Client::AsyncCallerContext> context = 
        Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    sprintf(buf, "%lu %lu", thread_id, fd);
    context->SetUUID(buf);
    
    S3Client.PutObjectAsync(request, PutObjectAsyncFinished, context);
    
    //恢复fd 的cache设置
    if (hos_info->mode & APPEND_MODE)
    {
        hos_info->cache = NULL;
        hos_info->cache_rest = hos_info->handle->cache_size;
        hos_info->cache_times = hos_info->handle->cache_times;
    }
    while (flag == 1)
    {
        return hos_write(fd, &stream[hos_info->cache_rest], rest, thread_id);
    }
    return HOS_CLIENT_OK;
}

int hos_close_fd(size_t fd, size_t thread_id)
{
    if (fd == 0)
    {
        return HOS_PARAMETER_ERROR;
    }

    delete_info_by_fd(&hash_hos_info[thread_id], fd);
    fd_info[thread_id][fd] = 0;

    return HOS_CLIENT_OK;
}

int hos_client_destory(hos_client_handle handle)
{
    size_t i = 0;
    if (handle == NULL)
    {
        return HOS_PARAMETER_ERROR;
    }

    delete handle->S3Client;

    Aws::Vector<Aws::S3::Model::Bucket>().swap(handle->buckets);
    Aws::ShutdownAPI((handle->options));

    for (i = 0; i < handle->thread_sum; i++)
    {
        delete_all(&hash_hos_info[i]);
    }
    //delete(handle->options);
    free(handle);

    return HOS_CLIENT_OK;
}
