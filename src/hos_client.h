/*************************************************************************
    > File Name: hos_client.h
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:13:59 PM CST
 ************************************************************************/
#ifndef __HOS_CLIENT_INIT__
#define __HOS_CLIENT_INIT__

#define HOS_CLIENT_OK 0

typedef struct hos_handle_s* hos_handle;

/* hos 错误码 */
enum hoserrors
{
    HOS_PARAMETER_ERROR = -1,
    HOS_FILE_NOT_EXIST = -2,
    HOS_HASH_NOT_FIND = -3,
    HOS_FD_NOT_ENOUGH = -4,
    HOS_SEND_FAILED = -5,
    HOS_RUNTIME_LOG_FAILED = -6,
    HOS_CONF_ERROR = -7,
    HOS_BUCKET_NOT_EXIST = -8,
    HOS_INSTANCE_NOT_INIT = -9,

};

/* s3 的错误码 */
enum s3errors
{
  //From Core Aws::S3::S3Error + 1//
  //////////////////////////////////////////////////////////////////////////////////////////
  INCOMPLETE_SIGNATURE = 1,
  INTERNAL_FAILURE = 2,
  INVALID_ACTION = 3,
  INVALID_CLIENT_TOKEN_ID = 4,
  INVALID_PARAMETER_COMBINATION = 5,
  INVALID_QUERY_PARAMETER = 6,
  INVALID_PARAMETER_VALUE = 7,
  MISSING_ACTION = 8, // SDK should never allow
  MISSING_AUTHENTICATION_TOKEN = 9, // SDK should never allow
  MISSING_PARAMETER = 10, // SDK should never allow
  OPT_IN_REQUIRED = 11,
  REQUEST_EXPIRED = 12,
  SERVICE_UNAVAILABLE = 13,
  THROTTLING = 14,
  VALIDATION = 15,
  ACCESS_DENIED = 16,
  RESOURCE_NOT_FOUND = 17,   // Shared with multiple services
  UNRECOGNIZED_CLIENT = 18,  // Most likely caused by an invalid access key or secret key
  MALFORMED_QUERY_STRING = 19,  
  SLOW_DOWN = 20,
  REQUEST_TIME_TOO_SKEWED = 21,
  INVALID_SIGNATURE = 22,
  SIGNATURE_DOES_NOT_MATCH = 23,
  INVALID_ACCESS_KEY_ID = 24,
  REQUEST_TIMEOUT = 25,
  NETWORK_CONNECTION = 100,   // General failure to send message to service

  // These are needed for logical reasons
  UNKNOWN = 101,
  CLIENT_SIGNING_FAILURE = 102, // Client failed to sign the request
  USER_CANCELLED = 103, // User cancelled the request
  SERVICE_EXTENSION_START_RANGE = 129,
  BUCKET_ALREADY_EXISTS= SERVICE_EXTENSION_START_RANGE + 1,
  BUCKET_ALREADY_OWNED_BY_YOU,
  NO_SUCH_BUCKET,
  NO_SUCH_KEY,
  NO_SUCH_UPLOAD,
  OBJECT_ALREADY_IN_ACTIVE_TIER,
  OBJECT_NOT_IN_ACTIVE_TIER
};

typedef void (*put_finished_callback)(bool result, const char *bucket, const char *object, const char *errmsg, void *userdata);

/*************************************************************************************
 * 函数名：   hos_init_instance
 * 输入参数： conf_path 配置文件路径
 *           const char *bucket, 上传的bucket
 * 返回值：   bool 初始化结果
*************************************************************************************/
bool hos_init_instance(const char *conf_path, const char *module, const char *bucket);
/*************************************************************************************
 * 函数名： hos_verify_instance
 * 返回值： bool 验证instance结果
*************************************************************************************/
bool hos_verify_instance();
/*************************************************************************************
 * 函数名： hos_upload_file
 * 参数:    const char * bucket             桶名称
 *          const char * file_path          上传对象路径
 *          put_finished_callback callback  upload操作结束时调用的回调函数
 *          void *userdata                  用户自定义数据
 * 返回值   int                              成功返回0，失败返回hoserros错误码
*************************************************************************************/

/* 只可以获取hos_init_instance */
const char *hos_get_init_instance_errmsg();
int hos_get_init_instance_errcode();
const char *hos_get_url_prefix();

int hos_upload_file(const char *bucket, const char *file_path, put_finished_callback callback, void* userdata);
/*************************************************************************************
 * 函数名： hos_upload_buf
 * 参数：   hos_instance instance           非空句柄
 *          const char * bucket             桶名称
 *          const char * object             上传对象名称
 *          const char *buf                 上传的buf
 *          size_t buf_len                  上传的buf的长度
 *          put_finished_callback callback  upload操作结束时调用的回调函数
 *          void *userdata                  用户自定义数据
 *          size_t thread_id                当前线程id
 * 返回值   int                              成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_upload_buf(const char *bucket, const char *object, const char *buf, size_t buf_len, put_finished_callback callback, void *userdata);
/*************************************************************************************
 * 函数名： hos_open_fd
 * 参数：   const char * bucket               桶名称
 *          const char * object               上传对象名称
 *          put_finished_callback callback    upload操作结束时调用的回调函数
 *          void *data                        用户回调函数自定义数据
 * 返回值   hos_handle                         失败返回NULL
*************************************************************************************/
int hos_open_handle(const char *bucket, const char *object, put_finished_callback callback, void *userdata, hos_handle *handle);
/*************************************************************************************
 * 函数名： hos_write
 * 参数：    hos_handle handle           hos_open_handle 返回的handle
 *          const char * stream         待上传的数据
 *          size_t stream               待上传的数据长度
 *          size_t position             append模式下的每段内容编号
 * 返回值   int                         成功返回0，失败返回hoserror
*************************************************************************************/
int hos_write(hos_handle handle, const char *stream, size_t stream_len);
/*************************************************************************************
 * 函数名： hos_close_handle
 * 参数：   hos_handle handle            hos_open_handle 返回的handle
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_close_handle(hos_handle handle);
/*************************************************************************************
 * 函数名： hos_shutdown_instance
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_shutdown_instance();
#endif
