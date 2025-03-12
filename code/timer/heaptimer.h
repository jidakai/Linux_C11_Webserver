/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"
//每个连接的客户端都有定时器，存放在小根堆中
typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {//将连接的客户端相关信息封装成TimerNode结构体
    int id;//文件描述符
    TimeStamp expires;//超时时间
    TimeoutCallBack cb;//回调函数关闭连接
    bool operator<(const TimerNode& t) {//重载
        return expires < t.expires;//比较超时时间
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);//调整--重建堆

    void add(int id, int timeOut, const TimeoutCallBack& cb);//为连接的客户端添加定时器

    void doWork(int id);

    void clear();

    void tick();  /* 清除超时结点 */

    void pop();

    int GetNextTick();//获取下个清除

private:
    void del_(size_t i);//删除
    
    void siftup_(size_t i);//插入上浮，先插入到最后

    bool siftdown_(size_t index, size_t n);//下浮操作，重建堆、建初堆使用

    void SwapNode_(size_t i, size_t j);//结点交换

//小根堆实现，保证连接时长最短的客户端在最前，方便处理
    std::vector<TimerNode> heap_;//vector容器-数组实现小根堆

    std::unordered_map<int, size_t> ref_;//保存位置下标和文件描述符的关系
};

#endif //HEAP_TIMER_H