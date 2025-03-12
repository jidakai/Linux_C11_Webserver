/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */
#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {//上浮操作
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;//父节点
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }//与父节点比较超时时间大小
        SwapNode_(i, j);//交换上浮，更新下标索引
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;//更新map容器中的下标索引-交换下标索引
    ref_[heap_[j].id] = j;
} 

bool HeapTimer::siftdown_(size_t index, size_t n) {//下浮操作
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {//为连接的客户端添加定时器
    assert(id >= 0);
    size_t i;
    //查找索引和文件描述符对应关系
    if(ref_.count(id) == 0) {//文件描述符是否存在
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();//尾部
        ref_[id] = i;//建立索引和文件描述符关联
        heap_.push_back({id, Clock::now() + MS(timeout), cb});//插入小根堆中
        siftup_(i);//向上调整
    } 
    else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);//延长时间
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {//下浮操作
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;//调整超时时间
    siftdown_(ref_[id], heap_.size());//下浮调整小根堆
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();//不为空判断是否超时
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { //判断超时时间和当前时间
            break; 
        }
        node.cb();//断开连接
        pop();//删除
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);//0即根为最少
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {//获取下个清除
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}