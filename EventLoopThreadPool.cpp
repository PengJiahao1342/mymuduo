#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <cstdio>
#include <memory>
#include <vector>

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop, const std::string& nameArg)
    : baseloop_(baseloop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // 不需要释放EventLoop
    // 它运行在EventLoopThread::threadFunc()栈上，出函数自动释放
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%02d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop()); // 底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
    }

    // 如果没有设置numThreads，那么整个服务端只有一个线程，运行baseloop，这个线程需要监听和处理发生的事件
    if (numThreads_ == 0 && cb) {
        cb(baseloop_);
    }
}

// 如果工作在多线程中，baseloop会默认以轮询方式分配Channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseloop_;

    if (!loops_.empty()) {
        // 通过轮询获取下一个处理事件的loop
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseloop_);
    } else {
        return loops_;
    }
}