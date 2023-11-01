#include "TcpClient.h"
#include "Callbacks.h"
#include "Connector.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Socket.h"

#include <cstdio>
#include <functional>
#include <mutex>
#include <string>



// 用于在析构函数中销毁连接
static void removeDstructConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    loop->queueInloop(std::bind(&TcpConnection::connectDestroyed, conn));
}

static void removeConnector(const ConnectorPtr& connector) { }

// 用于构造函数检查传给loop_的参数是否为空指针
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL("%s-%s-%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpClient::TcpClient(EventLoop* loop,
    const InetAddress& serverAddr,
    const std::string& nameArg)
    : loop_(CheckLoopNotNull(loop))
    , connector_(new Connector(loop, serverAddr))
    , name_(nameArg)
    , retry_(false)
    , connect_(true)
    , nextConnId_(1)
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
{
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
    LOG_INFO("TcpClient::TcpClient[%s] - connector %p\n", name_.c_str(), connector_.get());
}

TcpClient::~TcpClient()
{
    LOG_INFO("TcpClient::~TcpClient[%s] - connector %p\n", name_.c_str(), connector_.get());

    TcpConnectionPtr conn;
    bool unique = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        unique = connection_.unique(); // 判断shared_ptr的引用计数是否为1
        conn = connection_;
    }

    if (conn) {
        CloseCallback cb = std::bind(removeDstructConnection, loop_, std::placeholders::_1);
        loop_->runInloop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if (unique) {
            conn->forceClose(); // 这个TcpConnection是唯一对象，强制关闭
        }
    } else {
        // 当前连接已经释放，停止Connector
        connector_->stop();
        loop_->runAfter(1, std::bind(&removeConnector, connector_));
    }
}

void TcpClient::connect()
{
    LOG_INFO("TcpClient::connect[%s] - connecting to %s\n",
        name_.c_str(), connector_->serverAddress().toIpPort().c_str());
    connect_ = true;
    connector_->start(); // 开始进行连接，成功后会connfd会保存到connector_的Channel中
}

void TcpClient::disconnect()
{
    connect_ = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 连接存在需要断开连接
        if (connection_) {
            // 关闭写端 => EPOLLHUP => channel回调closecallback
            // => TcpServer::removeConnection => TcpConnection::connectDestroyed
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_ = false;
    connector_->stop(); // 断开连接后尝试重连
}

// connector的newConnection回调函数
void TcpClient::newConnection(int sockfd)
{
    // 获取对端地址client对端是服务器
    InetAddress peerAddr(Socket::getPeerAddr(sockfd));

    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    // 获取本地地址
    InetAddress localAddr(Socket::getLocalAddr(sockfd));

    // 这里创建TcpConnection对象时把sockfd封装进TcpConnection的Channel中
    // 所以之前在connector中需要先把绑定sockfd的Channel解绑清空，再返回sockfd
    // 这里创建的TcpConnection才是与服务器直接交互的
    TcpConnectionPtr conn(new TcpConnection(loop_,
        connName,
        sockfd,
        localAddr,
        peerAddr));
    // 下面的回调都是用户设置给TcpClient
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 关闭连接的回调TcpClient::removeConnection => TcpConnection::connectDestroyed
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));

    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    // connectEstablished建立连接
    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_.reset(); // TcpConnection指针清空
    }

    loop_->queueInloop(std::bind(&TcpConnection::connectDestroyed, conn));

    // 设置为重连并且还在连接状态，就restart重置连接
    if (retry_ && connect_) {
        LOG_INFO("TcpClient::connect[%s] - Reconnecting to %s\n",
            name_.c_str(), connector_->serverAddress().toIpPort().c_str());
        connector_->restart();
    }
}
