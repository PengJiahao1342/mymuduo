#include <functional>
#include <iostream>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>

#include <string>

class EchoServer {
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : loop_(loop)
        , server_(loop, addr, name)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量
        server_.setThreadNum(6);
    }

    void start() { server_.start(); }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        } else {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件的回调
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString(); // 提取缓冲区所有数据
        conn->send(msg);
        conn->shutdown(); // 关闭写端 EPOLLHUP=>closeCallback_
    }
    EventLoop* loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(7890);
    // LOG_INFO("Server IP Port : %s:%d\n", "127.0.0.1", 19980);
    EchoServer server(&loop, addr, "Maple Server"); // Acceptor--createsockfd--bind
    server.start(); // 启动loopthread listen listenfd打包成acceptChannel注册到mainloop
    loop.loop(); // 启动mainloop底层Poller epoll_wait监听事件

    return 0;
}