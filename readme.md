# Hos-client-cpp 简介
Hos-client-cpp 是基于aws s3的一组异步上传数据的API。  
这组API继承了aws s3的bucket和object的概念。其主要功能就是往hos服务器上指定的Bucket里面选择一个Object（可以是新建的）上传数据。  
# 概念简介
## Bucket
Bucket 就像是电脑里面的某一个顶层分区。所有的Object都必须保存在某一个 bucket 下面。  
或者可以把Bucket类比成文件系统里面的文件夹或者目录。 
Bucket在Hos服务器里面是唯一的。
## Object
Bucket 里面每一个存储的数据就是Object，由对象名（key），和数据（value）组成。  
Object的键（Key）可以很长，甚至按照一定前缀格式来指定，从而模拟文件夹的层级结构，比如 Protocol/Mail-Smtp/2020-11-25.pcap。
## Signature
连接Hos服务器需要验证，Hos系统采取Aws S3的验证方式：Signature Version 4 （客户端使用）和 Token（网页使用）。  
Signature Version 4需要两个参数，accesskeyid 和secretkey。
# 简单用法
1. 创建一个hos client

        hos_client_handle handle = hos_client_create("192.168.40.223", 9098, "default", "default", 100);

2. 创建一个bucket
    一个进程只允许有一个hos client，即使调用多次hos_create_bucket，返回的仍然是同一个client handle

        char *bucket_name = "myBucket";
        hos_client_handle handle = hos_create_bucket(handle, bucket_name);

3. 向Hos服务器上传内容
    文件上传的过程中允许用户注册一个callback函数，该函数会在client上传数据之后，收到服务器响应时被调用。
    callback示例如下：

        void callback(bool result, const char *error, const char *bucket, const char *object, void *userdata)
        {
            if (result)
            {
                printf("upload %s to %s successed!\n", object, bucket);
            }else
            {
                printf("upload %s to %s failed!\n", object, bucket);
                printf("error:%s", error);
            }
        }

    本文之后示例的callback都默认为当前的callback函数。
    userdata为为用户预留的自定义数据指针。
    callback也允许为NULL，即client不关心服务端的响应。

    1. 完整上传  
        1. 文件模式

                char *bucket = "myBucket";
                char *object = "myObject";
                char *file = "./mail.pcap";
                size_t thread_id = 0;
                void *userdata = NULL;
                hos_upload_file(handle, bucket, object, file, callback, userdata, thread_id);

        2. buffer模式

                char *bucket = "myBucket";
                char *object = "myObject";
                char *buff = "hello Hos!";
                size_t thread_id = 0;
                void *userdata = NULL;
                hos_upload_buff(handle, bucket, object, buff, buff_len, callback, userdata, thread_id);


    2. 分段上传

            int mode = BUFF_MODE | APPEND_MODE;
            char *bucket = "myBucket";
            char *object = "myObject";
            size_t thread_id = 0;
            void *userdata = NULL;
            char *stream = "hello Hos!";
            size_t fd = hos_open_fd(handle, bucket, object, callback, userdata, thread_id, mode);
            hos_write(fd, stream, strlen(stream), thread_id);
            hos_close_fd(fd, thread_id);


    fd的这种模式也可以实现完整上传，只需要在mode的时候不带APPEND_MODE标识则为完整上传模式。另外，文件模式不支持分段上传。
    fd模式可以分为以下几种：

                int mode = BUFF_MODE; //完整上传buff
                int mode = BUFF_MODE | APPEND_MODE; //分段上传buff
                int mode = FILE_MODE; //完整上传文件
    
    即使在FILE_MODE模式下添加上APPEND_MODE也不会进行分段上传，只会完整上传，同时也不会报错。

4. 销毁hos client

        hos_client_destory(handle);

# 高级用法
## Hos 线程池简介
由于Hos Client在往服务器上传数据的时候采用的是异步上传的方式，所以在触发上传操作的时候其实是将当前的上传操作递交给另一个线程进行的。
这个线程不是临时创建的，而是从一开始就初始化好的线程池里取出来的线程。如果短时间内同时上传大量object，那么很有可能导致线程池里没有多余的线程用来进行上传操作，导致当前的上传请求被丢弃。
为了在不影响用户体验的情况下，尽量避免出现这种情况就提出了Hos的缓存机制。

## Hos 缓存机制简介
Hos 的缓存机制只存在于
Hos client 提供了三个API来设置一些特殊参数。
1. hos_set_cache_times
2. hos_set_cache_size
3. hos_set_thread_sum
