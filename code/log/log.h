/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level, const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;//日志路径长度
    static const int LOG_NAME_LEN = 256;//文件名称长度
    static const int MAX_LINES = 50000;//日志的行，超过重新开辟

    const char* path_;//路径
    const char* suffix_;//前缀

    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool isOpen_;
 
    Buffer buff_;//临时缓冲区
    int level_;//级别，限制操作
    bool isAsync_;//是否异步

    FILE* fp_;//文件指针
    std::unique_ptr<BlockDeque<std::string>> deque_; //阻塞任务队列
    std::unique_ptr<std::thread> writeThread_;//从任务队列中读取任务的线程
    std::mutex mtx_;//互斥锁
};
//宏函数执行，获取实例，写日志、刷新日志
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);
//不同级别的日志处理
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H