#include "CheckHosClient.h"

void CheckStructHosConfigT(hos_config_t *actual, hos_config_t *expect)
{
    EXPECT_STREQ(actual->accesskeyid, expect->accesskeyid);
    EXPECT_STREQ(actual->secretkey, expect->secretkey);
    EXPECT_STREQ(actual->log_path, expect->log_path);
    EXPECT_EQ(actual->log_level, expect->log_level);
    EXPECT_EQ(actual->pool_thread_size, expect->pool_thread_size);
    EXPECT_EQ(actual->cache_count, expect->cache_count);
    EXPECT_EQ(actual->cache_size, expect->cache_size);
    EXPECT_EQ(actual->fs2_fmt, expect->fs2_fmt);
    EXPECT_STREQ(actual->fs2_ip, expect->fs2_ip);
    EXPECT_STREQ(actual->fs2_path, expect->fs2_path);
    EXPECT_EQ(actual->fs2_port, expect->fs2_port);
    EXPECT_STREQ(actual->ip, expect->ip);
    EXPECT_EQ(actual->port, expect->port);
    EXPECT_EQ(actual->thread_num, expect->thread_num);
    EXPECT_EQ(actual->max_request_context, expect->max_request_context);
    EXPECT_EQ(actual->max_request_num, expect->max_request_num);
}

void CheckStructFs2DataInfo(data_info_t *actual, data_info_t *expect, int thread_num)
{
    int i = 0;
    
    for (i = 0; i < thread_num; i++)
    {
        if (actual->cache)
        {
            EXPECT_EQ(actual->cache[i], expect->cache[i]);
        }
        if (actual->rx_bytes)
        {
            EXPECT_EQ(actual->rx_bytes[i], expect->rx_bytes[i]);
        }
        if (actual->rx_pkts)
        {
            EXPECT_EQ(actual->rx_pkts[i], expect->rx_pkts[i]);
        }
        if (actual->tx_bytes)
        {
        EXPECT_EQ(actual->tx_bytes[i], expect->tx_bytes[i]);
        }
        if (actual->tx_pkts)
        {
            EXPECT_EQ(actual->tx_pkts[i], expect->tx_pkts[i]);
        }
        if (actual->tx_failed_bytes)
        {
            EXPECT_EQ(actual->tx_failed_bytes[i], expect->tx_failed_bytes[i]);
        }
        if (actual->tx_failed_pkts)
        {
            EXPECT_EQ(actual->tx_failed_pkts[i], expect->tx_failed_pkts[i]);
        }
    }
}

void CheckStructHosFunc(hos_func_thread_t *actual, hos_func_thread_t *expect, int thread_num)
{
    //EXPECT_EQ(actual->fd_thread, expect->fd_thread);
    EXPECT_EQ(actual->fd_thread_status, expect->fd_thread_status);
    if (actual->fs2_info.reserved != NULL)
    {
        CheckStructFs2DataInfo((data_info_t *)actual->fs2_info.reserved,
            (data_info_t *)expect->fs2_info.reserved, thread_num);
    }
    EXPECT_EQ(actual->fs2_status, expect->fs2_status);
    //EXPECT_EQ(actual->fs2_thread, expect->fs2_thread);
}

void CheckStructGHosHandle(hos_client_handle_t *actual, hos_client_handle_t *expect)
{
    //EXPECT_STREQ(actual->buckets.c_str(), expect->buckets.c_str());
    int bucketNum = actual->buckets.size() > expect->buckets.size() ? expect->buckets.size() : actual->buckets.size();
    for (int i = 0; i < bucketNum; i++)
    {
        EXPECT_STREQ(actual->buckets.at(i).GetName().c_str(), expect->buckets.at(i).GetName().c_str());
    }
    EXPECT_EQ(actual->count, expect->count);
    //EXPECT_TRUE(actual->log != NULL);
    //EXPECT_TRUE(actual->S3Client != NULL);
    CheckStructHosConfigT(&actual->hos_config, &expect->hos_config);
    CheckStructHosFunc(&actual->hos_func, &expect->hos_func, actual->hos_config.thread_num);
}

void CheckStructGHosFdContext(hos_fd_context_t *actual, hos_fd_context_t *expect)
{
    if (actual == NULL || expect == NULL)
    {
        EXPECT_TRUE(actual == expect);
    }
    else
    {
        EXPECT_STREQ(actual->bucket, expect->bucket);
        //EXPECT_TRUE(actual->cache == NULL); 
        EXPECT_EQ(actual->cache_count, expect->cache_count);
        EXPECT_EQ(actual->cache_rest, expect->cache_rest);
        EXPECT_EQ(actual->callback, expect->callback);
        EXPECT_EQ(actual->fd_status, expect->fd_status);
        EXPECT_EQ(actual->mode, expect->mode);
        EXPECT_STREQ(actual->object, expect->object);
        EXPECT_EQ(actual->position, expect->position);
        EXPECT_EQ(actual->recive_cnt, expect->recive_cnt);
        EXPECT_EQ(actual->userdata, expect->userdata);
    }
}

void CheckHosInstance(hos_instance actual, hos_instance expect)
{
    EXPECT_EQ(actual->status, expect->status);
    EXPECT_EQ(actual->error_code, expect->error_code);
    EXPECT_STREQ(actual->error_message, expect->error_message);
    EXPECT_STREQ(actual->hos_url_prefix, expect->hos_url_prefix);
}