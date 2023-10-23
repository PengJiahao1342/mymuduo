#ifndef TIMERID_H
#define TIMERID_H

#pragma once
#include <cstdint>

class Timer;

// 封装一个Timer和其序列号，并且将TimerQueue设置为友元
class TimerId {
public:
    TimerId()
        : timer_(nullptr)
        , sequence_(0)
    {
    }
    TimerId(Timer* timer, int64_t seq)
        : timer_(timer)
        , sequence_(seq)
    {
    }

    // 把TimerQueue设置为友元
    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_;
};

#endif