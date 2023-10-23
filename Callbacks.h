#ifndef CALLBACKS_H
#define CALLBACKS_H

#pragma once
#include <cstddef>
#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;

// TcpConnection回调
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr)>;
using CloseCallback = std::function<void(const TcpConnectionPtr)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr)>;
using MessageCallback = std::function<void(const TcpConnectionPtr, Buffer*, Timestamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr, size_t)>;

// 定时器回调
using TimerCallback = std::function<void()>;

#endif