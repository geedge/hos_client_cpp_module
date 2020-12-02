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
## 完整上传
完整上传，顾名思义就是一个object一次性上传完整。
## 分段上传
分段上传与Aws S3的分段上传有所区别。Aws S3的完整上传是将内容分成几部分上传到服务器，并由客户端上传开始和结束的标识以及整个上传过程中的序号给服务器进行验证，只有服务器验证完整之后才会在服务器上生成这个object，否则机会丢弃整个上传的内容。Hos Client改进分段上传机制，Hos Client 上传的时候只需要标识这是一个分段上传的object，并且在上传的时候Hos client会自动给每次上传的内容生成一个序号标识，用来给服务器保证内容的完整性和有序性。这种分段上传机制，在一开始上传的时候就会在服务器生成一个object，之后的上传的内容都是在这个object上进行append操作。
## fd
Hos Client衍生出来的概念。这个fd可以类比为C语言标准库的文件描述符。Hos Client的fd标识一次完整的Object的上传过程。fd的是从3开始，上限最大是65535。
fd存在三种状态：REGISTER、INJECT、FREE。
REGISTER：fd可以正常进行缓存上传操作和处理服务器响应
INJECT：fd已经不用于缓存上传，只用于处理服务器响应
FREE：fd 处于空闲状态，等待REGISTER，不用于其他任何操作。
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
Hos 的缓存机制只存在于分段上传模式中。分段上传过程中，通过设置cache_size参数来启动Hos的缓存机制。cache_size的作用是设置一个最大的缓存大小，只有待上传的内容大于等于cache_size的时候才换触发上传操作。除了cache_size之外，还有一个cahce_count参数用来辅助缓存机制的。cache_count的作用是，防止多次缓存却一直满足不了cache_size大小，导致缓存长时间存在而不上传的情况出现。cache_size和cahce_count，只要有一个满足条件就会立即触发上传。
Hos client 提供了API来设置这两个参数
hos_set_cache_size 和 hos_set_cache_count。这两个API都是作用于整个客户端，必须在hos_open_fd之前使用。在调用者两个API之前获取到的fd，不会对这两个配置生效。size的单位是Byte。
示例：

        int mode = BUFF_MODE | APPEND_MODE;
        char *bucket = "myBucket";
        char *object = "myObject";
        size_t thread_id = 0;
        void *userdata = NULL;
        char *stream = "hello Hos!";
        hos_set_cache_size(handle, 100);
        hos_set_cache_count(handle, 100);
        size_t fd = hos_open_fd(handle, bucket, object, callback, userdata, thread_id, mode);
        hos_write(fd, stream, strlen(stream), thread_id);
        hos_close_fd(fd, thread_id);    
        
## fieldstat2 显示
Hos Client使用fieldstat2 进行信息监控，监控内容包括：收发的pkts和bytes，fd的使用情况，缓存的使用情况
Hos client 通过使用hos_expand_fs2来启用fieldstat2功能。
示例：

        fs2_path = "./log/fs2";
        int format = 0;
        char fs2_serverip = "127.0.0.1";
        char fs2_serverport = 10086;
        hos_expand_fs2(handle, fs2_path, format, "127.0.0.1", 10086);

其中fs2_path为本地日志落地路径，日志以天为一个周期，如果fs2_path为空则屏显输出；format 为0表示以默认输出，1表示Json输出；
fs2_serverip 和 port表示日志需要发送去的地方。

