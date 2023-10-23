#ifndef TIMERQUEUE_H
#define TIMERQUEUE_H

#pragma once
#include "Callbacks.h"
#include "Channel.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

class EventLoop;
class Timer;
class TimerId;

// 一个TimerQueue关联一个EventLoop，一个TimerQueue绑定一个文件描述符timerfd和封装他的Channel
// TimerQueue保存EventLoop中的Timer，根据超时时间从小到大放入集合中
// 最小的超时时间设置为timerfd的超时时间，到时间后timerfd有读事件发生
// 回调该Timer的定时处理函数
class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 添加定时器 一般在其他线程调用
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);

    // timerfd有读事件到来回调这个函数
    void handleRead();

    // 获取所有超时的定时器
    std::vector<Entry> getExpired(Timestamp now);

    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;

    ActiveTimerSet activeTimers_;
    std::atomic_bool callingExpiredTimers_;
    ActiveTimerSet cancelingTimers_;
};

#endif