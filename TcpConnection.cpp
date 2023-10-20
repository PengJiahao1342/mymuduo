#include "TcpConnection.h"
#include "Callbacks.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// 用于构造函数检查传给loop_的参数是否为空指针
static EventLoop*
CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL("%s-%s-%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
    const std::string& nameArg,
    int sockfd,
    const InetAddress& localAddr,
    const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , hightWaterMark_(64 * 1024 * 1024) // 64M
{
    // 给Channel设置相应的回调函数，Poller通知Channel感兴趣的事件发送了，Channel会回调相应的操作函数
    channel_->setReadCallBack(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallBack(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallBack(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallBack(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::connector[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::disconnector[%s] at fd=%d state=%d\n",
        name_.c_str(), channel_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(const std::string& buf)
{
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        } else {
            loop_->runInloop(std::bind(&TcpConnection::sendInLoop, shared_from_this(), buf.c_str(), buf.size()));
        }
    }
}

// 发送数据 应用写的快，内核发送数据较慢，需要把待发送数据写入缓冲区，并且设置了水位回调
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0; // 已发送字节数
    size_t remaining = len; // 未发送字节数
    bool faultError = false;

    if (state_ == kDisconnected) { // 之前这个connection调用过shutdown关闭，不能发送数据
        LOG_ERROR("disconnected, give up writing!\n");
        return;
    }

    // 一开始注册的Channel都是对reading感兴趣，监听EPOLLIN事件，没有监听EPOLLOUT事件
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        // 发送数据
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) { // 发送成功
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                // 数据一次性发送完毕，直接执行回调
                // 并且不需要再给Channel注册EPOLLOUT事件
                loop_->queueInloop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else { // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) { // SIGPIPE RESET
                    faultError = true;
                }
            }
        }
    }

    // 说明当前这次的write没有把数据全部发送出去，剩余的数据需要保存到outputBuffer_缓冲区中，
    // 并且个Channel注册EPOLLOUT事件，由于Poller工作在LT模式，只要TCP的发送缓冲区有空间，
    // 会通知相应的socket即Channel EPOLLOUT事件，调用Channel的writeCallback_回调，
    // 即绑定的TcpConnection::handleWrite，直到数据全部发送完成
    if (!faultError && remaining > 0) {
        // 当前outputBuffer_缓冲区中剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= hightWaterMark_
            && oldLen < hightWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInloop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        // remaining的数据写入缓冲区
        outputBuffer_.append((char*)(data) + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting(); // 注册Channel的写事件，Poller会给Channel通知EPOLLOUT事件
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInloop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) { // 说明outputBuffer_中的数据全部发送完毕
        // 关闭写端 会触发EPOLLHUP事件，在Channel中有判断
        // (revents_ & EPOLLHUP) && !(revents_ & EPOLLIN) 回调closeCallback_
        // 即初始化TcpConnection时绑定的TcpConnection::handleClose
        socket_->shutdownWrite();
    }
}

// 建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this()); // 记录TcpConnection对象，保证没有释放才进行操作
    channel_->enableReading(); // 向Poller注册Channel的EPOLLIN事件

    // 新连接建立，执行连接回调
    connectionCallback_(shared_from_this());
}

// 销毁连接
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll(); // 从Poller中del掉Channel所有感兴趣事件

        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 在Poller中删除Channel
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        // 已建立连接的用户，有可读事件发生，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) { // 连接关闭
        handleClose();
    } else {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead error\n");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                // 表示这一轮已经读取完缓冲区的数据写入完成
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    // 唤醒loop_对应的thread线程，执行回调
                    // 不过TcpConnection是属于某个subloop，一个subloop属于一个thread，
                    // 能够调用到TcpConnection::handleWrite应该loop_就在自己的thread
                    loop_->queueInloop(std::bind(writeCompleteCallback_, shared_from_this()));
                }

                if (state_ == kDisconnecting) {
                    // TcpConnection状态是正在关闭，写完这一轮数据就关闭连接
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite error\n");
        }
    } else { // Channel不可写
        LOG_ERROR("TcpConnection fd = %d is down, no more writing\n", channel_->fd());
    }
}

// Poller有事件通知Channel要执行closeCallback=>回调TcpConnection::handleClose
// =>回调TcpServer::removeConnection=>回调TcpConnection::connectDestroyed
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd = %d state = %d\n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll(); // Channel对任何事件不感兴趣并从Poller中删除

    TcpConnectionPtr connPtr(shared_from_this()); // connPtr指向当前TcpConnection对象的指针
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr); // 关闭连接的回调 回调TcpServer::removeConnection
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    int err = 0;
    // 检查socket是否真的发生错误
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno; // 确实发生错误
    } else {
        err = optval;
    }

    LOG_ERROR("TcpConnection::handleError name: %s - SO_ERROR: %d\n", name_.c_str(), err);
}