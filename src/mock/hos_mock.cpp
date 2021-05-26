#include <aws/core/utils/Outcome.h>
#include <aws/core/auth/AWSAuthSigner.h>
#include <aws/core/client/CoreErrors.h>
#include <aws/core/client/RetryStrategy.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/xml/XmlSerializer.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/core/utils/DNS.h>
#include <aws/core/utils/logging/LogMacros.h>

#include <aws/core/utils/event/EventStream.h>
#include "hos_mock.h"
#include <aws/s3/S3Endpoint.h>
#include <aws/s3/S3ErrorMarshaller.h>
#include <aws/s3/S3ARN.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CopyObjectRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/DeleteBucketRequest.h>
#include <aws/s3/model/DeleteBucketAnalyticsConfigurationRequest.h>
#include <aws/s3/model/DeleteBucketCorsRequest.h>
#include <aws/s3/model/DeleteBucketEncryptionRequest.h>
#include <aws/s3/model/DeleteBucketInventoryConfigurationRequest.h>
#include <aws/s3/model/DeleteBucketLifecycleRequest.h>
#include <aws/s3/model/DeleteBucketMetricsConfigurationRequest.h>
#include <aws/s3/model/DeleteBucketPolicyRequest.h>
#include <aws/s3/model/DeleteBucketReplicationRequest.h>
#include <aws/s3/model/DeleteBucketTaggingRequest.h>
#include <aws/s3/model/DeleteBucketWebsiteRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/DeleteObjectTaggingRequest.h>
#include <aws/s3/model/DeleteObjectsRequest.h>
#include <aws/s3/model/DeletePublicAccessBlockRequest.h>
#include <aws/s3/model/GetBucketAccelerateConfigurationRequest.h>
#include <aws/s3/model/GetBucketAclRequest.h>
#include <aws/s3/model/GetBucketAnalyticsConfigurationRequest.h>
#include <aws/s3/model/GetBucketCorsRequest.h>
#include <aws/s3/model/GetBucketEncryptionRequest.h>
#include <aws/s3/model/GetBucketInventoryConfigurationRequest.h>
#include <aws/s3/model/GetBucketLifecycleConfigurationRequest.h>
#include <aws/s3/model/GetBucketLocationRequest.h>
#include <aws/s3/model/GetBucketLoggingRequest.h>
#include <aws/s3/model/GetBucketMetricsConfigurationRequest.h>
#include <aws/s3/model/GetBucketNotificationConfigurationRequest.h>
#include <aws/s3/model/GetBucketPolicyRequest.h>
#include <aws/s3/model/GetBucketPolicyStatusRequest.h>
#include <aws/s3/model/GetBucketReplicationRequest.h>
#include <aws/s3/model/GetBucketRequestPaymentRequest.h>
#include <aws/s3/model/GetBucketTaggingRequest.h>
#include <aws/s3/model/GetBucketVersioningRequest.h>
#include <aws/s3/model/GetBucketWebsiteRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/GetObjectAclRequest.h>
#include <aws/s3/model/GetObjectLegalHoldRequest.h>
#include <aws/s3/model/GetObjectLockConfigurationRequest.h>
#include <aws/s3/model/GetObjectRetentionRequest.h>
#include <aws/s3/model/GetObjectTaggingRequest.h>
#include <aws/s3/model/GetObjectTorrentRequest.h>
#include <aws/s3/model/GetPublicAccessBlockRequest.h>
#include <aws/s3/model/HeadBucketRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/ListBucketAnalyticsConfigurationsRequest.h>
#include <aws/s3/model/ListBucketInventoryConfigurationsRequest.h>
#include <aws/s3/model/ListBucketMetricsConfigurationsRequest.h>
#include <aws/s3/model/ListMultipartUploadsRequest.h>
#include <aws/s3/model/ListObjectVersionsRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/ListPartsRequest.h>
#include <aws/s3/model/PutBucketAccelerateConfigurationRequest.h>
#include <aws/s3/model/PutBucketAclRequest.h>
#include <aws/s3/model/PutBucketAnalyticsConfigurationRequest.h>
#include <aws/s3/model/PutBucketCorsRequest.h>
#include <aws/s3/model/PutBucketEncryptionRequest.h>
#include <aws/s3/model/PutBucketInventoryConfigurationRequest.h>
#include <aws/s3/model/PutBucketLifecycleConfigurationRequest.h>
#include <aws/s3/model/PutBucketLoggingRequest.h>
#include <aws/s3/model/PutBucketMetricsConfigurationRequest.h>
#include <aws/s3/model/PutBucketNotificationConfigurationRequest.h>
#include <aws/s3/model/PutBucketPolicyRequest.h>
#include <aws/s3/model/PutBucketReplicationRequest.h>
#include <aws/s3/model/PutBucketRequestPaymentRequest.h>
#include <aws/s3/model/PutBucketTaggingRequest.h>
#include <aws/s3/model/PutBucketVersioningRequest.h>
#include <aws/s3/model/PutBucketWebsiteRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/PutObjectAclRequest.h>
#include <aws/s3/model/PutObjectLegalHoldRequest.h>
#include <aws/s3/model/PutObjectLockConfigurationRequest.h>
#include <aws/s3/model/PutObjectRetentionRequest.h>
#include <aws/s3/model/PutObjectTaggingRequest.h>
#include <aws/s3/model/PutPublicAccessBlockRequest.h>
#include <aws/s3/model/RestoreObjectRequest.h>
#include <aws/s3/model/SelectObjectContentRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/UploadPartCopyRequest.h>

using namespace Aws;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::S3;
using namespace Aws::S3::Model;
using namespace Aws::Http;
using namespace Aws::Utils::Xml;

S3ClientMock::S3ClientMock(const AWSCredentials &credentials, const Client::ClientConfiguration &clientConfiguration, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads, bool useVirtualAddressing)
{
    Aws::String accessKeyId = credentials.GetAWSAccessKeyId();
    Aws::String secretKey = credentials.GetAWSSecretKey();
    m_hosBuckets.push_back(Bucket().WithName("session_record_hos_bucket"));
    m_hosBuckets.push_back(Bucket().WithName("firewall_hos_bucket"));

    init(clientConfiguration);
    if (memcmp(clientConfiguration.endpointOverride.c_str(), "http://127.0.0.1:9098/hos/", clientConfiguration.endpointOverride.length()) != 0)
    {
        m_errorCode = S3Errors::NETWORK_CONNECTION;
    }
    if (memcmp(clientConfiguration.endpointOverride.c_str(), "127.0.0.2", clientConfiguration.endpointOverride.length()) == 0)
    {
        m_mockMode = 2;
        m_errorCode = S3Errors::REQUEST_TIMEOUT;
    }
    if ((int)m_errorCode == 0)
    {
        if (memcmp(accessKeyId.c_str(), "default", accessKeyId.length()) != 0)
        {
            m_errorCode = S3Errors::INVALID_ACCESS_KEY_ID;
        }
        else if (memcmp(secretKey.c_str(), "default", secretKey.length()) != 0)
        {
            m_errorCode = S3Errors::INVALID_SIGNATURE;
        }
    }
}

S3ClientMock::~S3ClientMock()
{
}

void S3ClientMock::init(const ClientConfiguration &config)
{
    SetServiceClientName("S3");
    if (config.endpointOverride.empty())
    {
    }
    else
    {
        OverrideEndpoint(config.endpointOverride);
    }
    m_errorCode = (Aws::S3::S3Errors)0;
}

ListBucketsOutcome S3ClientMock::ListBuckets() const
{
    switch (m_errorCode)
    {
    case (S3Errors)0:
        return ListBucketsOutcome(Aws::S3::Model::ListBucketsResult().WithBuckets(m_hosBuckets));
    case S3Errors::NETWORK_CONNECTION:
        return ListBucketsOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::NETWORK_CONNECTION, "NETWORK_CONNECTION", "curlCode: 7, Couldn't connect to server", false));
    case S3Errors::REQUEST_TIMEOUT:
        return ListBucketsOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::REQUEST_TIMEOUT, "REQUEST_TIMEOUT", "RequestTimeout", false));
    case S3Errors::INVALID_ACCESS_KEY_ID:
        return ListBucketsOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::INVALID_ACCESS_KEY_ID, "INVALID_ACCESS_KEY_ID", "invalid_access_key_id", false));
    case S3Errors::INVALID_SIGNATURE:
        return ListBucketsOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::INVALID_SIGNATURE, "INVALID_SIGNATURE", "Ivalid Signature", false));
    default:
        return ListBucketsOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::UNKNOWN, "UNKNOWN", "unknown", false));
    }
}

CreateBucketOutcome S3ClientMock::CreateBucket(const CreateBucketRequest &request) const
{
    return CreateBucketOutcome();
}

PutObjectOutcome S3ClientMock::PutObject(const PutObjectRequest &request) const
{
    if (!request.BucketHasBeenSet())
    {
        AWS_LOGSTREAM_ERROR("PutObject", "Required field: Bucket, is not set");
        return PutObjectOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::MISSING_PARAMETER, "MISSING_PARAMETER", "Missing required field [Bucket]", false));
    }
    if (!request.KeyHasBeenSet())
    {
        AWS_LOGSTREAM_ERROR("PutObject", "Required field: Key, is not set");
        return PutObjectOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::MISSING_PARAMETER, "MISSING_PARAMETER", "Missing required field [Key]", false));
    }

    auto bucket = request.GetBucket().c_str();
    for (auto &new_bucket : m_hosBuckets)
    {
        if (strcmp(new_bucket.GetName().c_str(), bucket) == 0)
        {
            return PutObjectOutcome(Aws::S3::Model::PutObjectResult());
        }
    }
    return PutObjectOutcome(Aws::Client::AWSError<S3Errors>(S3Errors::NO_SUCH_BUCKET, "NO_SUCH_BUCKET", "The specified bucket does not exist.", false));
}

bool S3ClientMock::PutObjectAsync(const PutObjectRequest &request, const PutObjectResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context) const
{
    handler(this, request, PutObject(request), context);
    return true;
}