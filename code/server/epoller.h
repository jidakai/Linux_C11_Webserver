/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */ 
#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);//最大检测数量

    ~Epoller();

    bool AddFd(int fd, uint32_t events);//添加事件

    bool ModFd(int fd, uint32_t events);//修改事件

    bool DelFd(int fd);//删除事件

    int Wait(int timeoutMs = -1);//检测事件

    int GetEventFd(size_t i) const;//获取就绪事件的文件描述符

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;//epoll_create创建epoll对象，返回的epoll描述符

    std::vector<struct epoll_event> events_;   //检测到的就绪事件 
};

#endif //EPOLLER_H