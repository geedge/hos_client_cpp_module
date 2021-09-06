#include <aws/s3/model/Bucket.h>
#include "CheckHosClient.h"

#define HOS_CONF "../conf/default.conf"
#define HOS_BUCKET "firewall_hos_bucket"

static void gtest_hos_handle_init(hos_client_handle_t *hos_handle, int thread_num)
{
    memset(hos_handle, 0, sizeof(hos_client_handle_t));
    hos_handle->buckets.push_back(Aws::S3::Model::Bucket().WithName("session_record_hos_bucket"));
    hos_handle->buckets.push_back(Aws::S3::Model::Bucket().WithName("firewall_hos_bucket"));
    hos_handle->count = 1;
    memcpy(hos_handle->hos_config.accesskeyid, "default", strlen("default")+1);
    memcpy(hos_handle->hos_config.secretkey, "default", strlen("default")+1);
    hos_handle->hos_config.cache_count = 10;
    hos_handle->hos_config.cache_size = 102400;
    hos_handle->hos_config.fs2_fmt = 0;
    memcpy(hos_handle->hos_config.fs2_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    memcpy(hos_handle->hos_config.fs2_path, "./log/hos_fs2_log", strlen("./log/hos_fs2_log")+1);
    hos_handle->hos_config.fs2_port = 10086;
    memcpy(hos_handle->hos_config.ip, "127.0.0.1", strlen("127.0.0.1")+1);
    hos_handle->hos_config.log_level = 30;
    memcpy(hos_handle->hos_config.log_path, "./hoslog", strlen("./hoslog")+1);
    hos_handle->hos_config.pool_thread_size = 10;
    hos_handle->hos_config.port = 9098;
    hos_handle->hos_config.thread_num = thread_num;
    hos_handle->hos_config.max_request_num = 100;
    hos_handle->hos_config.max_request_context = 10240000;
    hos_handle->hos_func.fd_thread_status = 0;
    hos_handle->hos_func.fs2_status = 1;

    data_info_t *data_info = (data_info_t *)calloc(1, sizeof(data_info_t));
    hos_handle->hos_func.fs2_info.reserved = (void *)data_info;
    data_info->tx_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->rx_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->rx_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_failed_pkts = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->tx_failed_bytes = (size_t *)calloc(thread_num, sizeof(size_t));
    data_info->cache = (size_t *)calloc(thread_num, sizeof(size_t));
}

static void gtest_hos_instance_init(hos_instance instance)
{
    memset(instance, 0, sizeof(hos_instance_s));
    instance->status = true;
    instance->error_code = 0;
    instance->error_message[0] ='\0';
    instance->hos_url_prefix = "http://127.0.0.1:9098/hos/";
}

static void gtest_hos_fd_init(hos_fd_context_t *fd_info)
{
    memset(fd_info, 0, sizeof(hos_fd_context_t));
    fd_info->bucket = (char *)HOS_BUCKET;
    fd_info->object = (char *)"object";
    fd_info->cache_count = 10;
    fd_info->cache_rest = g_hos_handle.hos_config.cache_size;
    fd_info->callback = NULL;
    fd_info->fd = 1;
    fd_info->fd_status = HOS_FD_REGISTER;
    fd_info->mode = BUFF_MODE | APPEND_MODE;
    fd_info->position = 0;
    fd_info->recive_cnt = 0;
    fd_info->userdata = NULL;
}

TEST(hos_open_fd, normal)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;
    hos_fd_context_t expect_fd_info[2];

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    size_t fd = hos_open_fd(HOS_BUCKET, "object", NULL, NULL, 0);
    EXPECT_EQ(fd, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[0]);
    CheckStructGHosFdContext(g_fd_context[0], &expect_fd_info[0]);
    
    size_t fd1 = hos_open_fd(HOS_BUCKET, "object", NULL, NULL, 1);
    EXPECT_EQ(fd1, 1);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    gtest_hos_fd_init(&expect_fd_info[1]);
    CheckStructGHosFdContext(g_fd_context[1], &expect_fd_info[1]);
    
    int ret = hos_close_fd(fd, 0);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[0], NULL);

    ret = hos_close_fd(fd1, 1);
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    //CheckStructGHosFdContext(g_fd_context[1], NULL);

    ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_open_fd, paramer_error)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    
    int fd = hos_open_fd(NULL, "object", NULL, NULL, 0);
    EXPECT_EQ(fd, HOS_PARAMETER_ERROR);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    CheckStructGHosFdContext(g_fd_context[0], NULL);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_open_fd, over_threadnums)
{
    hos_instance_s expect_hos_instance;
    hos_client_handle_t expect_hos_handle;

    hos_instance hos_instance = hos_init_instance(HOS_CONF, "hos_default_conf", 2, HOS_BUCKET);
    gtest_hos_instance_init(&expect_hos_instance);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    gtest_hos_handle_init(&expect_hos_handle, 2);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);

    int fd = hos_open_fd(HOS_CONF, "object", NULL, NULL, 3);
    EXPECT_EQ(fd, HOS_PARAMETER_ERROR);
    CheckHosInstance(hos_instance, &expect_hos_instance);
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    CheckStructGHosFdContext(g_fd_context[2], NULL);

    int ret = hos_shutdown_instance();
    EXPECT_EQ(ret, HOS_CLIENT_OK);
    expect_hos_instance.status = 0;
    expect_hos_instance.hos_url_prefix = NULL;
    CheckHosInstance(hos_instance, &expect_hos_instance);

    Aws::Vector<Aws::S3::Model::Bucket>().swap(g_hos_handle.buckets);
    data_info_t *data_info = (data_info_t *)expect_hos_handle.hos_func.fs2_info.reserved;
    free(data_info->cache);
    free(data_info->rx_bytes);
    free(data_info->rx_pkts);
    free(data_info->tx_bytes);
    free(data_info->tx_pkts);
    free(data_info->tx_failed_bytes);
    free(data_info->tx_failed_pkts);
    memset(&expect_hos_handle, 0, sizeof(hos_client_handle_s));
    CheckStructGHosHandle(&g_hos_handle, &expect_hos_handle);
    EXPECT_EQ((void *)g_fd_context, (void *)NULL);
}

TEST(hos_open_fd, not_init_instance)
{
    int fd = hos_open_fd(HOS_CONF, "object", NULL, NULL, 0);
    EXPECT_EQ(fd, HOS_INSTANCE_NOT_INIT);
}