#include <aws/s3/S3Client.h>

namespace Aws
{
    namespace S3
    {
        class AWS_S3_API S3ClientMock : public S3Client
        {
        public:
            S3ClientMock(const Aws::Auth::AWSCredentials &credentials, const Aws::Client::ClientConfiguration &clientConfiguration, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads, bool useVirtualAddressing);
            ~S3ClientMock();
            Model::ListBucketsOutcome ListBuckets() const;
            Model::CreateBucketOutcome CreateBucket(const Model::CreateBucketRequest& request) const;
            Model::PutObjectOutcome PutObject(const Model::PutObjectRequest& request) const;
            bool PutObjectAsync(const Model::PutObjectRequest& request, const PutObjectResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        private:
            void init(const Aws::Client::ClientConfiguration &ClientConfiguration);
            size_t m_mockMode;
            S3Errors m_errorCode;
            Aws::Vector<Model::Bucket> m_hosBuckets;
        }; // class S3ClientMock
    }      // namespace S3
}
