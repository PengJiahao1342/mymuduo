#ifndef LOGGER_H
#define LOGGER_H

#pragma once

#include "noncopyable.h"

#include <string>

//
// 用户不需要指定具体怎么调接口写日志, 直接定义宏方便用户写
// 宏定义时写do while(0)防止一些错误，##__VA_ARGS__是可变参数列表
// 使用格式
// LOG_INFO("%s %d", arg1, arg2...)
//
#define LOG_INFO(logmsgFormat, ...)                       \
    do {                                                  \
        Logger& logger = Logger::instance();              \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = { 0 };                           \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                      \
    do {                                                  \
        Logger& logger = Logger::instance();              \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = { 0 };                           \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                      \
    do {                                                  \
        Logger& logger = Logger::instance();              \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = { 0 };                           \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0)

#ifdef OPENDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do {                                                  \
        Logger& logger = Logger::instance();              \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = { 0 };                           \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

/*
 * 定义日志的级别
 * INFO ： 一般流程信息
 * ERROR ： 一般错误信息
 * FATAL ： 致命错误，导致进程退出，core信息
 * DEBUG ： 调试信息，调试时打开，默认关闭
 */
enum LogLevel {
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// 定义一个日志类，继承noncopyable，不需要拷贝构造和赋值
// 采用懒汉单例模式，在成员函数中用静态局部变量初始化，线程安全
class Logger : noncopyable {
public:
    // 获取日志唯一的实力对象接口
    static Logger& instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    int logLevel_;
    Logger() {}; // 构造函数私有化
};

#endif