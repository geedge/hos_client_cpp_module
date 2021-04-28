#include <aws/external/gtest.h>
#include "hos_client.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "hos_test_bucket"
#define HOS_BUFF "This a googleTest"
#define HOS_FILE "../conf/default.conf"

static void hos_callback(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}
static void hos_write_buff_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}
static void hos_write_append_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}
static void hos_write_file_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, true);
    EXPECT_STREQ(bucket, HOS_BUCKET);
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, NULL);
}
static void hos_bucket_not_exits_cb(bool result, const char *bucket, const char *object, const char *error, void *userdata)
{
    SUCCEED();
    EXPECT_EQ(result, false);
    EXPECT_STREQ(bucket, "bucket_not_exits");
    EXPECT_STREQ(object, (char *)userdata);
    EXPECT_STREQ(error, "The specified bucket does not exist.");
}

TEST(hos_write, normal)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 3, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    size_t fd = hos_open_fd(HOS_BUCKET, "object_buff", hos_write_buff_cb, (void *)"object_buff", 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd1 = hos_open_fd(HOS_BUCKET, "object_append", hos_write_append_cb, (void *)"object_append", 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd2 = hos_open_fd(HOS_BUCKET, "object_file", hos_write_file_cb, (void *)"object_file", 2, FILE_MODE);
    EXPECT_EQ(fd2, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd1, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd2, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 3, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    size_t fd = hos_open_fd("bucket_not_exits", "object_buff", hos_bucket_not_exits_cb, (void *)"object_buff", 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd1 = hos_open_fd("bucket_not_exits", "object_append", hos_bucket_not_exits_cb, (void *)"object_append", 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd2 = hos_open_fd("bucket_not_exits", "object_file", hos_bucket_not_exits_cb, (void *)"object_file", 2, FILE_MODE);
    EXPECT_EQ(fd2, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd1, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd2, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, sync_mode)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_sync_conf", 3, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    size_t fd = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd1 = hos_open_fd(HOS_BUCKET, "object_append", NULL, NULL, 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd2 = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 2, FILE_MODE);
    EXPECT_EQ(fd2, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_BUFF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd1, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd2, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, sync_mode_bucket_not_exits)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_sync_conf", 3, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    size_t fd = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(fd, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd1 = hos_open_fd(HOS_BUCKET, "object_append", NULL, NULL, 1, BUFF_MODE | APPEND_MODE);
    EXPECT_EQ(fd1, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd1, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    size_t fd2 = hos_open_fd(HOS_BUCKET, "object_buff", NULL, NULL, 2, FILE_MODE);
    EXPECT_EQ(fd2, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_write(fd2, HOS_FILE, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd1, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_close_fd(fd2, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, paramer_error)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int fd = hos_open_fd(NULL, "object", hos_callback, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, HOS_PARAMETER_ERROR);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(0, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, fd_not_find)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_HASH_NOT_FIND);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, file_not_exit)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int fd = hos_open_fd(HOS_CONF, "object", NULL, NULL, 0, FILE_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(3, "not_exit_file", strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_FILE_NOT_EXIST);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, over_threadnums)
{
    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int fd = hos_open_fd(HOS_CONF, "object", NULL, NULL, 0, BUFF_MODE);
    EXPECT_EQ(fd, 3);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 6);
    EXPECT_EQ(ret, HOS_PARAMETER_ERROR);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);
}

TEST(hos_write, not_init_instance)
{
    int ret = hos_write(3, HOS_BUFF, strlen(HOS_CONF), 0);
    EXPECT_EQ(ret, HOS_INSTANCE_NOT_INIT);
}
static void *hos_function(void *ptr)
{
    #define HOS_FD_NUMS_LOCAL 20
    //int thread_id = (int)ptr;
    size_t thread_id = reinterpret_cast<size_t>(ptr);
    hos_instance hos_instance = NULL;
    int i = 0;
    int fd[HOS_FD_NUMS_LOCAL] = {0};
    char object[HOS_FD_NUMS_LOCAL][1024];
    int ret = 0;

    {
        hos_instance = hos_get_instance();
        if (hos_instance->result == false)
        {
            hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 32, HOS_BUCKET);
        }
    }
    EXPECT_EQ(hos_instance->result, true);
    EXPECT_EQ(hos_instance->error_code, HOS_CLIENT_OK);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");

    for (i = 0; i < 20; i++)
    {
        snprintf(object[i], 1024, "object_%lu_%d", thread_id, i);
        fd[i] = hos_open_fd(HOS_CONF, object[i], hos_callback, object[i], 0, BUFF_MODE | APPEND_MODE);
        EXPECT_EQ(fd[i], i+3);
        EXPECT_EQ(hos_instance->error_code, 0);
        EXPECT_STREQ(hos_instance->error_message, "");
        EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    }

    for (i = 0; i < 20; i++)
    {
        ret = hos_write(fd[i], HOS_BUFF, strlen(HOS_BUFF), i);
        EXPECT_EQ(ret, HOS_CLIENT_OK);
        EXPECT_EQ(hos_instance->error_code, 0);
        EXPECT_STREQ(hos_instance->error_message, "");
        EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    }

    for (i = 0; i < 20; i++)
    {
        ret = hos_close_fd(fd[i], i);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(hos_instance->error_code, 0);
        EXPECT_STREQ(hos_instance->error_message, "");
        EXPECT_STREQ(hos_instance->hos_url_prefix, "http://192.168.44.12:9098/hos/");
    }

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(hos_instance->error_code, 0);
    EXPECT_STREQ(hos_instance->error_message, "");
    EXPECT_STREQ(hos_instance->hos_url_prefix, NULL);

    pthread_exit(NULL);
#undef HOS_FD_NUMS_LOCAL
}

TEST(hos_write, mutil_thread)
{
    pthread_t thread_num[32];

    for (size_t i = 0; i < 32; i++)
    {
        pthread_create(&thread_num[i], NULL, hos_function, (void *)i);
    }
}