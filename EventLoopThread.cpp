#include "EventLoopThread.h"
#include "EventLoop.h"
#include <mutex>

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层的新线程

    EventLoop* loop = nullptr;
    {
        // 这里的线程和执行下面threadFunc的线程不是同一个线程，需要等待子线程通知
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) {
            cond_.wait(lock);
        }
        loop = loop_;
    }

    return loop;
}

// 下面的方法在启动的新线程中运行
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的EventLoop，和上面创建的新线程一一对应，one loop per thread

    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop loop -> Poller.poll

    // loop返回即EventLoop退出
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}