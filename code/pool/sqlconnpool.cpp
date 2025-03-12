/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */

#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {//构造初始化
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {//懒汉式单例，调用时创建
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {//循环池子中连接数量
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);//创建数据库对象
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        //连接数据库
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        connQue_.push(sql);//初始化连接后添加连接的数据库对象到连接队列，方便后续操作
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);//初始化信号量实现同步互斥
}

MYSQL* SqlConnPool::GetConn() {//从连接队列中获取一个连接对象，互斥获取
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);//使用信号量P，无资源则阻塞，等待有时唤醒
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();//获取连接对象
        connQue_.pop();//出连接队列
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);//提供新的连接对象，插入连接队列中
    sem_post(&semId_);//提供资源，信号量V
}

void SqlConnPool::ClosePool() {//关闭连接
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();//取出连接队列中连接对象
        mysql_close(item);//关闭连接
    }
    mysql_library_end();        
}

int SqlConnPool::GetFreeConnCount() {//当前可用的数据库连接对象
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
