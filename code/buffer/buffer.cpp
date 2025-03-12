/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}//初始化缓冲区

size_t Buffer::ReadableBytes() const {//向TCP缓存区写数据相当于从客户端缓冲区中读出数据，获取当前的客户端对象缓冲区buffer_可读的大小
    return writePos_ - readPos_;//客户端缓冲区写的位置-当前读的位置
}
size_t Buffer::WritableBytes() const {//读TCP缓冲区数据相当于向客户端读缓冲区中写入数据，获取当前的客户端对象的缓冲区buffer_可写的大小
    return buffer_.size() - writePos_;//缓冲区总大小-已经读取的数据/已经写入读缓存区的数据
}

size_t Buffer::PrependableBytes() const {
    return readPos_;//已经从客户端缓冲区中读出的数据，即前面可以用的空间
}

const char* Buffer::Peek() const {//确定读的开始位置
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {//当前写位置
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {//更新写的位置
    writePos_ += len;
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}
// Append(buff, len - writable);
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);//确定读写空间是否足够，不够进行扩容
    std::copy(str, str + len, BeginWrite());//将临时数据写入扩容后的空间
    HasWritten(len);//更新写的位置
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {//空间是否足够
        MakeSpace_(len);//不够创建新空间
    }
    assert(WritableBytes() >= len);
}
//自增长缓冲区在不断的交互过程中会趋于稳定，每个客户端对象中都有缓冲区
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    //读取文件描述符标识的TCP缓冲区的数据，存放在客户端对象中的读缓冲区
    char buff[65535];//临时存放数据，保证能够将所有数据读取
    struct iovec iov[2];
    const size_t writable = WritableBytes();//获取客户端对象的读缓冲区buffer_大小
    /* 分散读，需要两个缓冲区，先读到客户端对象的缓冲区buffer_中，
    不够则读到临时存放数据的缓冲区buff，保证数据全部读完，再进行扩容 */
    iov[0].iov_base = BeginPtr_() + writePos_;//读取数据到读缓冲区的起始地址，BeginPtr_()代表初始位置
    //writePos_代表当前的读取偏移，即已经读了多少数据
    iov[0].iov_len = writable;//客户端对象的读缓冲区buffer_大小
    iov[1].iov_base = buff;//临时缓存区
    iov[1].iov_len = sizeof(buff); //临时缓冲区大小

    const ssize_t len = readv(fd, iov, 2);//分散读，两个缓冲区
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {//读到的字节数小，客户端的读缓冲区可自行处理
        writePos_ += len;//更新读缓冲区个数writePos_
    }
    else {//读到的字节数大，客户端的读缓冲区不够，使用临时缓冲区buff处理
        writePos_ = buffer_.size();//读客户端读缓冲区用完
        Append(buff, len - writable);//剩下的交由buff读
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

void Buffer::MakeSpace_(size_t len) {//创建新空间
    if(WritableBytes() + PrependableBytes() < len) {//原缓冲区中存放数据的空间不够
        buffer_.resize(writePos_ + len + 1);//更新缓冲区的大小，扩容
    } 
    else {//空间足够，将数据前移
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}