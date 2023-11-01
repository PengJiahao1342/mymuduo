#include <bits/types/time_t.h>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <endian.h>
#include <functional>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <mymuduo/Buffer.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/InetAddress.h>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpClient.h>
#include <mymuduo/Timestamp.h>

// 用于解析服务器发送32位二进制数的时间
class ChargenClient {
public:
    ChargenClient(EventLoop* loop, const InetAddress& serverAddr)
        : loop_(loop)
        , client_(loop, serverAddr, "ChargenClient")
    {
        client_.setConnectionCallback(
            std::bind(&ChargenClient::onConnection, this, std::placeholders::_1));
        client_.setMessageCallback(
            std::bind(&ChargenClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void connect() { client_.connect(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("TimeServer - %s -> %s is UP.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());

        } else {
            LOG_INFO("TimeServer - %s -> %s is DOWN.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
            loop_->quit();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receivetime)
    {
        buf->retrieveAll();
    }

    EventLoop* loop_;
    TcpClient client_;
};

int main(int argc, char* argv[])
{
    LOG_INFO("pid = %d\n", getpid());

    if (argc > 1) {

        EventLoop loop;
        InetAddress serverAddr(7890, argv[1]);

        ChargenClient chargenclient(&loop, serverAddr);
        chargenclient.connect();
        loop.loop();
    } else {
        printf("Usage: %s host_ip\n", argv[0]);
    }
    return 0;
}