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
    static Timestamp now();
    std::string toString() const; // 只读方法

private:
    int64_t microSecondsSinceEpoch_;
};

#endif