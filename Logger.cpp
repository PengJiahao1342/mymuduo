#include "Logger.h"
#include "Timestamp.h"

#include <iostream>
#include <ostream>

// 获取日志唯一的实力对象接口
Logger& Logger::instance()
{
    static Logger logger; // 在成员函数中用静态局部变量初始化，线程安全
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

/*
 * 写日志 格式
 * [日志级别] time : msg
 */
void Logger::log(std::string msg)
{
    switch (logLevel_) {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    std::cout << Timestamp::now().toString()
              << " : " << msg << std::endl;
}