#ifndef EVENTLOOPTHREAD_H
#define EVENTLOOPTHREAD_H

#pragma once
#include "Thread.h"
#include "noncopyable.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

class EventLoop;

// 一个线程有一个事件循环 在这里绑定EventLoop和Thread
class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};

#endif