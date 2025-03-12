/*
 * @Author       : jk
 * @Date         : 2023-06-15
 * @copyleft Apache 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {//保存客户端的连接信息，键值为文件描述符，实值为客户端连接信息
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {//出错关闭连接
    response_.UnmapFile();//内存释放
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;//关闭减一
        close(fd_);//关闭文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {//读取事件的数据
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);//读取文件描述符标识的缓冲区的数据，存放在客户端对象中的读缓冲区
        if (len <= 0) {
            break;
        }
    } while (isET);//判断是否ET，是则需要循环读完
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    //分散写
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}
//业务逻辑处理，解析读缓冲区中的HTTP请求数据->生成HTTP响应并存入写缓冲区
bool HttpConn::process() {
    request_.Init();//初始化request请求类
    if(readBuff_.ReadableBytes() <= 0) {//判断缓冲区是否有数据
        return false;
    }
    else if(request_.parse(readBuff_)) {//有数据则解析readBuff_缓冲区将数据封装在request_对象中
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);//初始化响应数据
    } else {
        response_.Init(srcDir, request_.path(), false, 400);//不成功返回400
    }

    response_.MakeResponse(writeBuff_);//生成响应信息，响应行和响应头存放在写缓冲区writeBuff_
    //响应体即具体的资源文件使用mmap映射到内存
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());//地址
    iov_[0].iov_len = writeBuff_.ReadableBytes();//长度
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();//资源位置
        iov_[1].iov_len = response_.FileLen();//资源长度
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
