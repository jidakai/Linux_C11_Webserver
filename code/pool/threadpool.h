/*
 * @Author       : jk
 * @Date         : 2023-06-18
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>//互斥锁
#include <condition_variable>//条件遍历
#include <queue>//容器
#include <thread>//c++线程库
#include <functional>//函数算法
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {//有参构造，创建并初始化pool_
            assert(threadCount > 0);
            //创建threadCount个子线程，并执行线程内部处理
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([pool = pool_] {//创建线程，lambda函数初始化线程
                    std::unique_lock<std::mutex> locker(pool->mtx);//获取线程中的锁
                    while(true) {
                        if(!pool->tasks.empty()) {//判断任务队列是否为空
                            auto task = std::move(pool->tasks.front());//从任务队列中取首个任务
                            pool->tasks.pop();//取出后移除队列中任务
                            locker.unlock();
                            task();//任务执行代码
                            locker.lock();//保证线程同步
                        } 
                        else if(pool->isClosed) break;//为空则判断线程池是否关闭，关闭则退出当前线程
                        else pool->cond.wait(locker);//任务队列为空且线程池未关闭释放锁并阻塞等待任务队列添加任务时唤醒
                    }
                }).detach();//线程分离，退出后自动回收
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {//析构函数
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;//关闭线程池
            }
            pool_->cond.notify_all();//唤醒所有线程，线程自动退出
        }
    }

    template<class F>
    void AddTask(F&& task) {//任务队列添加任务并唤醒线程池阻塞线程
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);//上锁
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();//唤醒一个阻塞线程
    }

private:
    //线程池结构体
    struct Pool {
        std::mutex mtx;//互斥锁
        std::condition_variable cond;//条件变量
        bool isClosed;//关闭
        std::queue<std::function<void()>> tasks;//共享的保存任务的队列
    };
    std::shared_ptr<Pool> pool_; //智能指针管理线程池，引用计数，pool_为一个指向Pool的智能指针
};


#endif //THREADPOOL_H