/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"
//面向对象的编程思想，抽象出客户端类，处理所有与客户端相关操作
class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);//读取客户端文件描述符事件的数据

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;
    
    sockaddr_in GetAddr() const;
    
    bool process();

    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }
//共享静态资源
    static bool isET;
    static const char* srcDir;//资源目录
    static std::atomic<int> userCount;//客户端连接数
    
private:
   
    int fd_;
    struct  sockaddr_in addr_;

    bool isClose_;
    
    int iovCnt_;
    struct iovec iov_[2];//分散I/O，处理多个非连续的缓冲区
    

    //*利用标准库容器封装char，实现自动增长的缓冲区；
    Buffer readBuff_; // 读缓冲区，保存请求数据
    Buffer writeBuff_; // 写缓冲区，保存响应数据

    HttpRequest request_;
    HttpResponse response_;
};


#endif //HTTP_CONN_H