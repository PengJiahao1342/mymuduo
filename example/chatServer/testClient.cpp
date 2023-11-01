#include "LengthHeaderCodec.h"

#include "mymuduo/EventLoopThread.h"
#include "mymuduo/TcpClient.h"
#include <mymuduo/Callbacks.h>

#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <functional>
#include <mutex>
#include <mymuduo/InetAddress.h>
#include <mymuduo/Logger.h>
#include <mymuduo/Timestamp.h>
#include <ostream>
#include <pthread.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

//
// 一个聊天服务器的客户端muduo实现
// 如何处理Tcp分包，以及涉及muduo多线程
//

class ChatClient : noncopyable {
public:
    ChatClient(EventLoop* loop, const InetAddress serverAddr)
        : client_(loop, serverAddr, "ChatClient")
        , codec_(std::bind(&ChatClient::onStringMessage, this,
              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
        client_.setConnectionCallback(std::bind(&ChatClient::onConnetion, this, std::placeholders::_1));

        // 与server一样，通过LengthHeaderCodec中间层进行数据的打包和解包
        client_.setMessageCallback(std::bind(&LengthHeaderCodec::onMessage, &codec_,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void connect() { client_.connect(); }
    void disconnect() { client_.disconnect(); }

    // 加锁是为了保护shared_ptr而不是TcpConnection
    void write(const std::string message)
    {
        std::unique_lock<std::mutex> lock(std::mutex);
        if (connection_) {
            codec_.send(connection_, message);
        }
    }

private:
    void onConnetion(const TcpConnectionPtr& conn)
    {
        // 加锁保护shared_ptr
        std::unique_lock<std::mutex> lock(mutex_);
        if (conn->connected()) {
            LOG_INFO("%s -> %s is up\n",
                conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str());

            connection_ = conn;

        } else {
            LOG_INFO("%s -> %s is down\n",
                conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str());

            connection_.reset();
        }
    }

    void onStringMessage(const TcpConnectionPtr& conn, const std::string msg, Timestamp receiveTime)
    {
        // 不能用cout，因为cout不是线程安全的，printf是线程安全的
        printf("<<< %s\n", msg.c_str());
    }

    TcpClient client_;
    LengthHeaderCodec codec_;
    std::mutex mutex_;
    TcpConnectionPtr connection_;
};

int main(int argc, char* argv[])
{
    LOG_INFO("pid = %d\n", getpid());

    if (argc > 2) {
        EventLoopThread loopthread;

        std::string ip = argv[1];
        u_int16_t port = static_cast<u_int16_t>(atoi(argv[2]));
        InetAddress serverAddr(port, ip);

        // 客户端不需要进行Poller监听，只需要创建出一个thread运行EventLoop即可，
        // 这个EventLoop专门处理这个客户端的连接
        ChatClient client(loopthread.startLoop(), serverAddr);
        client.connect();

        std::string line;
        while (std::getline(std::cin, line)) {
            client.write(line);
        }

        client.disconnect();

        // 等待连接断开
        int64_t usec = 1000 * 1000;
        struct timespec ts = { 0, 0 };
        ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
        ::nanosleep(&ts, NULL);

    } else {
        LOG_INFO("Usage: %s host_ip port\n", argv[0]);
    }

    return 0;
}