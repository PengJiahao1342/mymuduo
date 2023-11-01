#include "LengthHeaderCodec.h"

#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mymuduo/Timestamp.h>
#include <string>
#include <unistd.h>
#include <unordered_set>

//
// 一个聊天服务器的服务端muduo实现
// 如何处理Tcp分包，以及涉及muduo多线程
// 分包：指如何发送方在发送消息时对数据进行一定处理，使得接受方能够从字节流中识别并还原出消息
// 这里采用的是在每个消息头部加一个长度字段，表示当前消息发送的字节数
//

class ChatServer : noncopyable {
public:
    ChatServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "ChatServer")
        , codec_(std::bind(&ChatServer::onStringMessage, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
        server_.setConnectionCallback(
            std::bind(&ChatServer::onConnetion, this, std::placeholders::_1));

        // server_的消息回调函数注册的是LengthHeaderCodec::onMessage，相当于一个间接层，用于处理数据解码
        // codec_中的回调又注册了ChatServer::onStringMessage，在解码完成后进行调用
        server_.setMessageCallback(
            std::bind(&LengthHeaderCodec::onMessage, &codec_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start() { server_.start(); }

private:
    void onConnetion(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("%s -> %s is up\n",
                conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str());

            // 有新连接到来就加入连接列表中
            connections_.insert(conn);

        } else {
            LOG_INFO("%s -> %s is down\n",
                conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str());

            // 连接断开则需要从列表中清除   避免内存泄漏
            connections_.erase(conn);
        }
    }

    // 遍历整个连接列表，把消息发给每个客户端
    void onStringMessage(const TcpConnectionPtr& conn, const std::string& message, Timestamp receiveTime)
    {
        for (auto& it : connections_) {
            codec_.send(it, message);
        }
    }

    using ConnectionList = std::unordered_set<TcpConnectionPtr>;

    TcpServer server_;
    LengthHeaderCodec codec_;
    ConnectionList connections_;
};

int main(int argc, char* argv[])
{
    LOG_INFO("pid = %d\n", getpid());

    if (argc > 1) {
        EventLoop loop;
        std::string ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        InetAddress serverAddr(port, ip);
        ChatServer server(&loop, serverAddr);
        server.start(); // 创建线程池并开始监听
        loop.loop(); // 开启事件循环，创建Poller并准备处理Channel事件
    } else {
        LOG_INFO("Usage %s ip port\n", argv[0]);
    }

    return 0;
}