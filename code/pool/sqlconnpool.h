/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */ 
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"
//数据库连接池，维护多个数据库连接对象
class SqlConnPool {
public:
    static SqlConnPool *Instance();//单例模式

    MYSQL *GetConn();//获取连接
    void FreeConn(MYSQL * conn);//释放连接，放回池子
    int GetFreeConnCount();//获取可连接空闲数量

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
//私有化构造
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;//最大的连接数
    int useCount_;//当前用户数
    int freeCount_;//空闲的用户，即当前可连接的用户数

    std::queue<MYSQL *> connQue_; //连接池主体--连接队列，保存连接的数据库对象，操作数据库
    std::mutex mtx_;//互斥锁
    sem_t semId_;//信号量
};


#endif // SQLCONNPOOL_H