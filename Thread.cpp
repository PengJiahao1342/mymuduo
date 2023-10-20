#include "Thread.h"
#include "CurrentThread.h"

#include <cstdio>
#include <memory>
#include <semaphore.h>
#include <thread>

std::atomic_int32_t Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_) {
        thread_->detach(); // thread类提供了设置分离线程的方法
    }
}

// 开启线程
// 一个Thread对象记录的就是一个新线程的详细信息
void Thread::start()
{
    started_ = true;
    sem_t sem; // 信号量
    sem_init(&sem, false, 0);

    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid(); // 获取线程id
        sem_post(&sem); // 信号量加1
        func_(); // 开启一个新线程，专门执行该线程函数
    }));

    // 线程与线程之间执行顺序不定，必须要等待获取上面线程的tid值
    sem_wait(&sem); // 等待信号
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32] = { 0 };
        snprintf(buf, sizeof buf, "Thread%02d", num);
        name_ = buf;
    }
}