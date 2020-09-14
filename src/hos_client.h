/*************************************************************************
    > File Name: hos_client_api.h
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:13:59 PM CST
 ************************************************************************/
#ifndef __HOS_CLIENT_INIT__
#define __HOS_CLIENT_INIT__

/*hos client 句柄*/
typedef void* hos_client_handle;

#define HOS_CLINET_OK 0
#define HOS_CLIENT_ERR -1

/* s3 的错误码 */
enum s3errors
{
  //From Core//
  //////////////////////////////////////////////////////////////////////////////////////////
  INCOMPLETE_SIGNATURE = 0,
  INTERNAL_FAILURE = 1,
  INVALID_ACTION = 2,
  INVALID_CLIENT_TOKEN_ID = 3,
  INVALID_PARAMETER_COMBINATION = 4,
  INVALID_QUERY_PARAMETER = 5,
  INVALID_PARAMETER_VALUE = 6,
  MISSING_ACTION = 7, // SDK should never allow
  MISSING_AUTHENTICATION_TOKEN = 8, // SDK should never allow
  MISSING_PARAMETER = 9, // SDK should never allow
  OPT_IN_REQUIRED = 10,
  REQUEST_EXPIRED = 11,
  SERVICE_UNAVAILABLE = 12,
  THROTTLING = 13,
  VALIDATION = 14,
  ACCESS_DENIED = 15,
  RESOURCE_NOT_FOUND = 16,   // Shared with multiple services
  UNRECOGNIZED_CLIENT = 17,  // Most likely caused by an invalid access key or secret key
  MALFORMED_QUERY_STRING = 18,  
  SLOW_DOWN = 19,
  REQUEST_TIME_TOO_SKEWED = 20,
  INVALID_SIGNATURE = 21,
  SIGNATURE_DOES_NOT_MATCH = 22,
  INVALID_ACCESS_KEY_ID = 23,
  REQUEST_TIMEOUT = 24,
  NETWORK_CONNECTION = 99,   // General failure to send message to service

  // These are needed for logical reasons
  UNKNOWN = 100,
  CLIENT_SIGNING_FAILURE = 101, // Client failed to sign the request
  USER_CANCELLED = 102, // User cancelled the request
  SERVICE_EXTENSION_START_RANGE = 128
  BUCKET_ALREADY_EXISTS= SERVICE_EXTENSION_START_RANGE + 1,
  BUCKET_ALREADY_OWNED_BY_YOU,
  NO_SUCH_BUCKET,
  NO_SUCH_KEY,
  NO_SUCH_UPLOAD,
  OBJECT_ALREADY_IN_ACTIVE_TIER,
  OBJECT_NOT_IN_ACTIVE_TIER
};

/*************************************************************************************
 * 函数名： hos_client_init
 * 参数：   const char *endpoint        目的地址，如”http://192.168.44.12:9098/hos“
 *          const char *accesskeyid     AWS access key ID，如”default“
 *          const char *secretkey       AWS secret key，如”secretkey“
 * 返回值： 成功返回一个非空句柄，失败返回NULL。（失败原因都是因为输入参数为空）
*************************************************************************************/
hos_client_handle hos_client_init(const char *endpoint, const char *accesskeyid, const char *secretkey);
/*************************************************************************************
 * 函数名： hos_create_bucket
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 * 返回值： bool 
*************************************************************************************/
bool hos_create_bucket(hos_client_handle handle, const char *bucket);
/*************************************************************************************
 * 函数名： hos_upload_async
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 *          const char * object         上传对象名称
 * 返回值   bool
*************************************************************************************/
bool hos_upload_async(hos_client_handle handle, const char *bucket, const char *object);
/*************************************************************************************
 * 函数名： hos_client_close
 * 参数：   hos_client_handle handle    非空句柄
*************************************************************************************/
void hos_client_close(hos_client_handle handle);
#endif
