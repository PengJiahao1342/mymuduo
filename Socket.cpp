#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <asm-generic/socket.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Socket::~Socket()
{
    close(sockfd_);
}

int Socket::createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("%s-%s-%d listen socket create error: %d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

void Socket::bindAddress(const InetAddress& loacladdr)
{
    if (0 != ::bind(sockfd_, (sockaddr*)loacladdr.getSockAddr(), sizeof(sockaddr_in))) {
        LOG_FATAL("bind sockfd: %d fail\n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024)) {
        LOG_FATAL("listen sockfd: %d fail\n", sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    memset(&addr, 0, sizeof addr);
    // 对connfd设置非阻塞
    // Reactor模型 Poller + non-blocking IO
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

// 关闭写端
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("shutdownWrite error\n");
    }
}

// 以下设置套接字选项
// 开启低延迟数据发送，Nagle算法一般会将小数据打包成一个较大数据段一次发送，避免网络拥塞
// 开启TCP_NODELAY会禁用Nagle算法
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

// 开启地址复用
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

// 开启端口复用
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
}

// 开启TCP keepalive
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}