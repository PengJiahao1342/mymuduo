#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#pragma once
#include "CurrentThread.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class Channel;
class Poller;

class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInloop(Functor cb); // 在当前loop中执行cb
    void queueInloop(Functor cb); // 把cb放入队列中，不在当前loop执行，唤醒loop所在线程，执行cb

    void wakeup(); // 唤醒loop所在线程

    // EventLoop的方法->Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead(); // waked up
    void doPendingFunctors(); // 处理回调函数

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_; // 标识退出loop循环

    const pid_t threadId_; // 记录当前loop的线程ID

    Timestamp pollReturnTime_; // poller返回发生事件的Channels的时间点
    std::unique_ptr<Poller> poller_; // 管理Poller的指针

    int wakeupFd_; // 主要作用，当mainLoop获取一个新用户的Channel，通过轮询算法选择一个subloop即Reactor，通过该成员变量唤醒subloop处理Channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所有回调操作
    std::mutex mutex_; // 互斥锁，用于保护vector容器的线程安全操作
};

#endif