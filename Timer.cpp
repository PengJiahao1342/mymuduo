#include "Timer.h"


std::atomic_int64_t s_numCreated_(0); // 静态变量类外初始化

// 重新设置超时时间
void Timer::restart(Timestamp now)
{
    if (repeat_) {
        expiration_ = addTime(now, interval_);
    }else {
        expiration_ = Timestamp::invalid();
    }
}