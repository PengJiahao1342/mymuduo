#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <utility>
#pragma once
#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = std::move(cb); }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();
    
    EventLoop* loop_; // Acceptor使用的是用户定义的baseloop，也即mainReactor
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};

#endif