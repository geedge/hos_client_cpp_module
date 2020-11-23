/*************************************************************************
    > File Name: hos_client_api.h
    > Author: pxz
    > Created Time: Thu 10 Sep 2020 03:13:59 PM CST
 ************************************************************************/
#ifndef __HOS_CLIENT_INIT__
#define __HOS_CLIENT_INIT__

/*hos client 句柄*/
typedef struct hos_client_handle_s* hos_client_handle;

#define HOS_CLIENT_OK 0

/* fd mode */
#define FILE_MODE   0x00
#define BUFF_MODE   0x01
#define APPEND_MODE 0x02    /* 默认不追加 */

/* hos 错误码 */
enum hoserrors
{
    HOS_PARAMETER_ERROR = -1,
    HOS_FILE_NOT_EXITS = -2,
    HOS_HASH_NOT_FIND = -3,
    HOS_FD_NOT_ENOUGH = -4,
    HOS_SEND_FAILED = -5,

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

typedef void (*put_finished_callback)(bool, const char *, const char *, const char *, void *);

/*************************************************************************************
 * 函数名： hos_client_init
 * 参数：   const char *endpoint        目的地址，如”http://192.168.44.12:9098/hos“
 *          const char *accesskeyid     AWS access key ID，如”default“
 *          const char *secretkey       AWS secret key，如”default“
 *          size_t thread_sum           线程总数
 * 返回值： 成功返回一个非空句柄，失败返回NULL。（失败原因都是因为输入参数不合法）
*************************************************************************************/
hos_client_handle hos_client_create(const char *endpoint, const char *accesskeyid, const char *secretkey, size_t pool_size);
/*************************************************************************************
 * 函数名： hos_create_bucket
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 * 返回值： int                         成功返回0，S3错误返回s3errors错误码，hos client错误返回hoserrors错误码
*************************************************************************************/
bool hos_verify_bucket(hos_client_handle handle, const char *bucket);
/*************************************************************************************
 * 函数名： hos_create_bucket
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 * 返回值： int                         成功返回0，S3错误返回s3errors错误码，hos client错误返回hoserrors错误码
*************************************************************************************/
int hos_create_bucket(hos_client_handle handle, const char *bucket);
/*************************************************************************************
 * 函数名： hos_set_cache_size
 * 参数：   hos_client_handle handle    非空句柄
 *          size_t cache_size           append 模式每次追加的buffer大小   
*************************************************************************************/
void hos_set_cache_size(hos_client_handle handle,  size_t cache_size);
/*************************************************************************************
 * 函数名： hos_set_cache_times
 * 参数：   hos_client_handle handle    非空句柄
 *          size_t cache_times           append 模式追加次数
*************************************************************************************/
void hos_set_cache_times(hos_client_handle handle,  size_t cache_times);
/*************************************************************************************
 * 函数名： hos_set_thread_sum
 * 参数：   hos_client_handle handle    非空句柄
 *          size_t thread_sum           append 模式追加次数
*************************************************************************************/
void hos_set_thread_sum(hos_client_handle handle,  size_t thread_sum);
/*************************************************************************************
 * 函数名： hos_upload_async
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 *          const char * file_path         上传对象路径
 *          put_finished_callback callback upload操作结束时调用的回调函数
 *          void *userdata              用户自定义数据
 *          size_t thread_id            当前线程id
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_upload_file(hos_client_handle handle, const char *bucket, const char *file_path, put_finished_callback callback, void* userdata, size_t thread_id);
/*************************************************************************************
 * 函数名： hos_upload_async
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 *          const char * object         上传对象名称
 *          const char *buf             上传的buf
 *          size_t buf_len              上传的buf的长度
 *          put_finished_callback callback upload操作结束时调用的回调函数
 *          void *userdata              用户自定义数据
 *          size_t thread_id            当前线程id
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_upload_buf(hos_client_handle handle, const char *bucket, const char *object, const char *buf, size_t buf_len, put_finished_callback callback, void *userdata, size_t thread_id);
/*************************************************************************************
 * 函数名： hos_open_fd
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * bucket         桶名称
 *          const char * object         上传对象名称
 *          put_finished_callback callback upload操作结束时调用的回调函数
 *          void *data                  用户自定义数据
 *          size_t thread_id            线程ID
 *          int mode                    模式 (FILE OR BUFFER, APPEND OR NOT)
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_open_fd(hos_client_handle handle, const char *bucket, const char *object, put_finished_callback callback, void *userdata, size_t thread_id, int mode);
/*************************************************************************************
 * 函数名： hos_upload_stream_async
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * stream         待上传的数据
 *          size_t stream               待上传的数据长度
 *          size_t thread_id            线程ID
 *          size_t position             append模式下的每段内容编号
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_write(size_t fd, const char *stream, size_t stream_len, size_t thread_id);
/*************************************************************************************
 * 函数名： hos_expand_fs2
 * 参数：   hos_client_handle handle    非空句柄
 *          const char * path           log 路径
 *          int format                  0:default; 1: Json
 *          char *server_ip             服务IP地址
 *          int port                    服务端口
*************************************************************************************/
void hos_expand_fs2(hos_client_handle handle, const char * path, int format, char *server_ip, int port);
/*************************************************************************************
 * 函数名： hos_close_fd
 * 参数：   size_t fd                   fd
 *          size_t thread_id            线程ID
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_close_fd(size_t fd, size_t thread_id);
/*************************************************************************************
 * 函数名： hos_client_destory
 * 参数：   hos_client_handle handle    非空句柄
 * 返回值   int                         成功返回0，失败返回hoserros错误码
*************************************************************************************/
int hos_client_destory(hos_client_handle handle);
#endif
