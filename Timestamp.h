#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#pragma once
#include <cstdint>
#include <string>

// 时间戳类，用于获取当前时间
class Timestamp {
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch); // explicit 防止int64_t与Timestamp类型隐式转换

    void swap(Timestamp& that)
    {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    static Timestamp now();
    static Timestamp invalid() { return Timestamp(); }

    bool valid() const { return microSecondsSinceEpoch_ > 0; }
    std::string toString() const; // 只读方法
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; };

    static const int kMicroSecondsPerSecond = 1000 * 1000; // 静态常量整型成员变量类内初始化

private:
    int64_t microSecondsSinceEpoch_;
};

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

// 重载<运算符，用于TimerQueue中进行超时时间的比较
// set中默认使用<运算符比较
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

// 重载==运算符，用于TimerQueue中进行超时时间的比较
inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

#endif