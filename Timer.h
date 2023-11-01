#ifndef TIMER_H
#define TIMER_H

#pragma once
#include "Callbacks.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <cstdint>
#include <utility>

// 定时器类
class Timer : noncopyable {
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > 0.0) // 有设置执行间隔即需要重复执行
        , sequence_(++s_numCreated_)
    {
    }

    ~Timer() { }

    void run() const { callback_(); }

    // 重新设置超时时间
    void restart(Timestamp now);

    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    static int64_t numCreated() { return s_numCreated_; }

private:
    const TimerCallback callback_; // 定时执行函数
    Timestamp expiration_; // 超时时间
    const double interval_; // 第一次超时后的执行间隔，为0表示一次性定时器，单位为s
    const bool repeat_; // 是否重复定时执行任务标志

    const int64_t sequence_; // 序列号 每个Timer有自己的序列号
    static std::atomic_int64_t s_numCreated_; // 这个原子变量的增加保证上面的序列号唯一
};

#endif