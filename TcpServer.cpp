#include "TcpServer.h"
#include "Logger.h"
#include "Socket.h"
#include "TcpConnection.h"

#include <cstdio>
#include <functional>
#include <netinet/in.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

// 用于构造函数检查传给loop_的参数是否为空指针
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL("%s-%s-%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
    , nextConnId_(1)
    , started_(0)
{
    // 有新用户连接时，执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    LOG_INFO("TcpServer::~TcpServer [%s] destructing\n", name_.c_str());

    for (auto& item : connections_) {
        // 这个局部shared_ptr智能指针对象出右括号自动释放
        TcpConnectionPtr conn(item.second);

        // ConnectionMap中的TcpConnectionPtr不再指向对象，可以释放
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInloop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置subloop个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0) { // started_原子操作，防止TcpServer对象start被创建多次
        threadPool_->start(threadInitCallback_); // 启动底层loop线程池
        loop_->runInloop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有新客户端连接，acceptor会执行这个回调
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 通过线程池的轮询算法选择一个subloop来管理这个Channel
    EventLoop* ioloop = threadPool_->getNextLoop();

    char buf[64] = { 0 };
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; // ConnId只在mainloop中++，没有线程安全问题
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机ip地址和端口号
    // sockaddr_in localaddr;
    // ::bzero(&localaddr, sizeof localaddr);
    // socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    // if (::getsockname(sockfd, (sockaddr*)&localaddr, &addrlen) < 0) {
    //     LOG_ERROR("sockets::getLoaclAddr\n");
    // }
    InetAddress localAddr(Socket::getLocalAddr(sockfd));

    // 根据连接成功的sockfd，创建TcpConnection对象  localAddr--服务器 peerAddr--客户端
    TcpConnectionPtr conn(new TcpConnection(
        ioloop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>注册到Poller=>notify Channel执行回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调 TcpServer::removeConnection
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // subloop直接调用TcpConnection::connectEstablished--注册EPOLLIN事件
    ioloop->runInloop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInloop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
        name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInloop(std::bind(&TcpConnection::connectDestroyed, conn));
}