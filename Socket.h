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

    int fd() const
    {
        return sockfd_;
    }
    void bindAddress(const InetAddress& loacladdr);
    void listen();
    int accept(InetAddress* peeraddr);
    int connect(const InetAddress* serverAddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

    static struct sockaddr_in getLocalAddr(int sockfd);
    static struct sockaddr_in getPeerAddr(int sockfd);
    static bool isSelfConnect(int sockfd);

    int getSocketError(int sockfd);

    void close(int sockfd);

private:
    const int sockfd_;
};

#endif