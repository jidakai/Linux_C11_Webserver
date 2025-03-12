/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */

#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    srcDir_ = getcwd(nullptr, 256);//获取当前工作路径-资源目录
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);//拼接路径到资源路径
    HttpConn::userCount = 0;//客户端连接后被封装为HttpConn对象，保存连接信息
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
//初始化epoll事件模式
    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true;}//初始化套接字

    if(openLog) {//是否打开日志
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);//获取单例，初始化
        //logQueSize异步日志的阻塞队列大小，为0使用同步日志
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {//初始化监听、已连接epoll事件模式
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;//设置EPOLLONESHOT，保证一个线程处理一个socket
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);//判断是否是ET模式
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {//服务器未关闭则循环运行
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();//获取超时时间，减少epoll调用次数
        }
        int eventCnt = epoller_->Wait(timeMS);//监听就绪事件，是否有事件到达
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            //Reactor模式：主线程负责监听文件描述符的事件以及接收新连接，数据的I/O交由工作线程完成
            //处理子线程时主线程也在正常执行
            int fd = epoller_->GetEventFd(i);//获取就绪事件文件描述符
            uint32_t events = epoller_->GetEvents(i);//获取就绪事件的事件代码
            if(fd == listenFd_) {//如果就绪事件有监听描述符则说明有新连接
                DealListen_();//处理监听事件-处理新连接，接收客户端连接
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);//出错关闭连接
            }
            else if(events & EPOLLIN) {//处理读操作
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {//处理写操作
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {//连接成功，但是超过可连接数量
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);//汇报错误
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {//出错关闭连接
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}        

void WebServer::AddClient_(int fd, sockaddr_in addr) {//添加监听的客户端，同时保存客户端的连接信息
    assert(fd > 0);
    users_[fd].init(fd, addr);//初始化HttpConn对象，保存客户端的连接信息到map容器中
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));//为连接的客户端添加定时器
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);//epoll添加监听的客户端，EPOLLIN事件
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {//主线程处理新连接
    struct sockaddr_in addr;//保存连接的客户端的信息包括端口和IP
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);//接受连接返回用于通信的文件描述符
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {//连接成功，但是超过可连接数量
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);//添加新的需要监听的客户端
    } while(listenEvent_ & EPOLLET);//ET模式需要一次性读完缓冲区数据，一次性处理好当前所有客户端的连接
}

void WebServer::DealRead_(HttpConn* client) {//工作子线程处理读操作
    assert(client);
    ExtentTime_(client);//有读写则延长超时时间
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));//向任务队列添加任务，唤醒线程池中的工作线程
}

void WebServer::DealWrite_(HttpConn* client) {//工作子线程处理写操作
    assert(client);
    ExtentTime_(client);//延长超时时间
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));//向任务队列添加任务，唤醒线程池中的工作线程
}
//延长超时时间
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {//子线程向TCP缓冲区执行读操作
    assert(client);
    int ret = -1;
    int readErrno = 0;
    //面向对象的编程思想，抽象出客户端类，处理所有与客户端相关操作
    ret = client->read(&readErrno);//读取文件描述符标识的缓冲区的数据，存放在客户端对象中的读缓冲区
    if(ret <= 0 && readErrno != EAGAIN) {//读取错误，非阻塞-1可能是EAGAIN是正常状态
        CloseConn_(client);
        return;
    }
    OnProcess(client);//子线程进行业务逻辑处理
}

void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);//处理成功，修改文件描述符为可写
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {//子线程向TCP缓冲区执行写操作
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);//客户端写缓冲区的数据写入TCP缓冲区
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;//结构体存储地址和端口
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);//字节序转化
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);//创建监听套接字
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));//服务器绑定本地IP和端口
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);//开始监听
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);//添加监听事件
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);//设置非阻塞
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);//设置非阻塞
}


