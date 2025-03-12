/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"
//面向对象的编程思想，抽象出服务器类，处理所有与服务器相关操作，提供服务器相关API
class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

private:
    bool InitSocket_(); //初始化套接字
    void InitEventMode_(int trigMode);////初始化监听、已连接epoll事件模式
    void AddClient_(int fd, sockaddr_in addr);//添加监听的客户端，同时保存客户端的连接信息
  
    void DealListen_();//主线程处理连接事件
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;//最大文件描述符个数

    static int SetFdNonblock(int fd);//设置文件描述符非阻塞

    int port_;//端口
    bool openLinger_;//是否打开优雅关闭
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;//是否关闭
    int listenFd_;//监听的文件描述符
    char* srcDir_;//资源目录
    
    uint32_t listenEvent_;//监听的文件描述符事件
    uint32_t connEvent_;//连接的文件描述符事件
   
    std::unique_ptr<HeapTimer> timer_;//定时器
    std::unique_ptr<ThreadPool> threadpool_;//线程池
    std::unique_ptr<Epoller> epoller_;//epoll对象
    std::unordered_map<int, HttpConn> users_;//map容器保存客户端连接信息，键值对，键值为文件描述符，实值为连接信息封装的HttpConn对象
};


#endif //WEBSERVER_H