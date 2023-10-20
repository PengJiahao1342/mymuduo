#ifndef SOCKET_H
#define SOCKET_H

#pragma once
#include "noncopyable.h"

class InetAddress;

// 封装socketfd
class Socket : noncopyable {
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }
    ~Socket();

    static int createNonblocking();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& loacladdr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};

#endif