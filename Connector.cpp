#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG("connector[%p]\n", this);
}

Connector::~Connector()
{
    LOG_DEBUG("destruct connector[%p]\n", this);
}

// 启动Connector 可以在任意线程调用
// start=>startInLoop=>connect
void Connector::start()
{
    connect_ = true;
    loop_->runInloop(std::bind(&Connector::startInLoop, shared_from_this()));
}

void Connector::startInLoop()
{
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG("don't connect\n");
    }
}

void Connector::connect()
{
    // 创建一个非阻塞socket保存到socket_
    socket_.reset(new Socket(Socket::createNonblocking()));
    // 尝试与服务器建立连接
    int ret = socket_->connect(&serverAddr_);

    // 根据错误类型进行连接或者重试
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        // 连接成功
        connecting(socket_->fd());
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        // 连接超时，非阻塞socket返回
        retry(socket_->fd());
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        LOG_ERROR("connect error in Connector::startInLoop errno=%d\n", savedErrno);
        socket_->close(socket_->fd());
        break;

    default:
        LOG_ERROR("Unexpected error in Connector::startInLoop errno=%d\n", savedErrno);
        socket_->close(socket_->fd());
        break;
    }
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);

    // 将sockfd封装进Channel并设置回调函数
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallBack(std::bind(&Connector::handleWrite, shared_from_this()));
    channel_->setErrorCallBack(std::bind(&Connector::handleError, shared_from_this()));
    // 开启EPOLLOUT写事件监听并添加到Poller中
    channel_->enableWriting();
}

void Connector::retry(int sockfd)
{
    // 先关闭之前的sockfd
    socket_->close(sockfd);
    // 设置成未连接状态
    setState(kDisconnected);

    // 超时时间后重连
    if (connect_) {
        LOG_INFO("Connector::retry - Retry connecting to %s in %d milliseconds.\n",
            serverAddr_.toIpPort().c_str(), retryDelayMs_);
        loop_->runAfter(retryDelayMs_ / 1000.0,
            std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);

    } else {
        LOG_DEBUG("don't connect\n");
    }
}

// 重置Connector 要重新初始化，必须在所在subloop线程调用
// retry是断线重连，在连接成功或者正在连接的情况下又断开连接setState(kDisconnected)，再多次尝试连接
// restart是重置连接，放弃之前的连接，从零开始连接，所以restart做初始化并startInLoop开始连接即可
void Connector::restart()
{
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

// 停止Connector 可以在任意线程调用
void Connector::stop()
{
    connect_ = false;
    loop_->queueInloop(std::bind(&Connector::stopInLoop, shared_from_this()));
}

void Connector::stopInLoop()
{
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::handleWrite()
{
    LOG_INFO("Connector::handleWrite state=%d\n", (int)state_);

    // 连接状态才能进行写，disConnected状态无法进入
    if (state_ == kConnecting) {
        // 连接成功后Channel的任务就完成了
        // 释放并且把sockfd保存，等待将sockfd所有权给TcpClient
        int sockfd = removeAndResetChannel();

        int err = socket_->getSocketError(sockfd);

        if (err) {
            // 确实有错误发生，重连
            LOG_ERROR("Connector::handleWrite - SO_ERROR = %d %s\n", err, std::strerror(err));
            retry(sockfd);
        } else if (socket_->isSelfConnect(sockfd)) { // 如果是连接到自身也是发生错误或者环回，重连
            LOG_ERROR("Connector::handleWrite - Self connect\n");
            retry(sockfd);
        } else { // 没有错误也并不是连接到自身 正常连接
            setState(kConnected); // 已连接状态
            if (connect_) {
                // 正常连接上，调用新连接回调，newConnectionCallback_由TcpClient传入
                // 会把sockfd绑定到一个新Channel，新Channel和之前用于连接的Channel属于同一个EventLoop，
                // 所以之前用于连接的Channel需要释放，否则会出现同一个sockfd绑定在两个Channel的情况
                newConnectionCallback_(sockfd);
            } else {
                socket_->close(sockfd);
            }
        }
    }
}

void Connector::handleError()
{
    LOG_ERROR("Connector::handleError state=%d\n", (int)state_);

    // 处于正在连接状态 即把sockfd封装进Channel的过程出现错误
    // 清除sockfd的Channel并且取消Poller绑定
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();

        int err = socket_->getSocketError(sockfd);

        LOG_INFO("SO_ERROR = %d %s\n", err, std::strerror(err));

        // 尝试重连
        retry(sockfd);
    }
}

// 清除sockfd的Channel并且取消Poller绑定
int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();

    // 在这里需要通过subloop来重置Channel，因为内置了Channel::handleEvent会自动处理
    loop_->queueInloop(std::bind(&Connector::resetChannel, shared_from_this()));
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset(); // 释放之前持有的对象
}