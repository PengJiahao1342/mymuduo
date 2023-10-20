#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Socket.h"

#include <asm-generic/errno-base.h>
#include <cerrno>
#include <functional>
#include <unistd.h>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(Socket::createNonblocking()) // socket
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr); // bind

    // Acceotor.listen有新用户连接时需要执行一个回调---将connfd打包成Channel输入到subReactor中
    acceptChannel_.setReadCallBack(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading(); // acceptChannel_ => Poller
}

// listenfd有事件发生，就是有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subloop，唤醒并分发当前新客户端的Channel
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR("%s-%s-%d accept error: %d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) {
            LOG_ERROR("%s-%s-%d accept reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}