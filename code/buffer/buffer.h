/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */

#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;  //可写     
    size_t ReadableBytes() const ; //可读
    size_t PrependableBytes() const;//追加

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();//首个字符
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);//创建新的空间，自增长
//不论读写缓冲区都有读的位置和写的位置，读缓冲区频繁改变写的位置，写缓冲区频繁改变读的位置
    std::vector<char> buffer_;//存放数据
    std::atomic<std::size_t> readPos_;//读的位置
    std::atomic<std::size_t> writePos_;//写的位置
};

#endif //BUFFER_H