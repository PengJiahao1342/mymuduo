#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#pragma once
#include "Callbacks.h"
#include "TcpConnection.h"
#include "noncopyable.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

class Connector;
class EventLoop;
class InetAddress;

using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable {
public:
    TcpClient(EventLoop* loop,
        const InetAddress& serverAddr,
        const std::string& nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection()
    { // 获取当前的Tcp连接
        std::unique_lock<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop* getLoop() const { return loop_; }
    bool retry() const { return retry_; }
    void enableRetry() { retry_ = true; }
    const std::string& name() const { return name_; }

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

private:
    // connector的newConnectionCallback_设置为TcpClient::newConnection
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_; // Connector指针
    const std::string name_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    std::atomic_bool retry_; // 是否重连标志
    std::atomic_bool connect_; // 是否连接标志

    int nextConnId_;
    std::mutex mutex_;
    TcpConnectionPtr connection_;
};

#endif