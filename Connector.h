#ifndef CONNECTOR_H
#define CONNECTOR_H

#pragma once
#include "InetAddress.h"
#include "noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>

class Channel;
class EventLoop;
class Socket;

//
// 类似于Acceptor封装listenfd用于监听客户端连接
// Connector被TcpClient调用，用于封装connfd的通信套接字
//
class Connector : noncopyable, public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    // 启动Connector 可以在任意线程调用
    void start();
    // 重置Connector 必须在所在subloop线程调用
    void restart();
    // 停止Connector 可以在任意线程调用
    void stop();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        newConnectionCallback_ = cb;
    }

    const InetAddress& serverAddress() const { return serverAddr_; }

private:
    enum States {
        kDisconnected,
        kConnecting,
        kConnected
    };

    void setState(States s) { state_ = s; }
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);

    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    static const int kMaxRetryDelayMs = 30 * 1000; // 尝试超时连接最大时间为30s
    static const int kInitRetryDelayMs = 500; // 最初重连时间为500ms 每次重连重连时间翻倍

    EventLoop* loop_;
    std::unique_ptr<Channel> channel_; // 这个Channel的任务只负责连接，连接完成后释放并且把sockfd给TcpClient
    std::unique_ptr<Socket> socket_; // socket_用于使用socket相关方法
    InetAddress serverAddr_;
    std::atomic_bool connect_;
    std::atomic<States> state_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_;
};

#endif