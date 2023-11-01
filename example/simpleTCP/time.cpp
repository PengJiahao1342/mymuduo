#include <bits/types/time_t.h>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <endian.h>
#include <functional>
#include <mymuduo/Buffer.h>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>
#include <string>
#include <unistd.h>

// 发送32位二进制数的时间
class TimeServer {
public:
    TimeServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "TimeServer")
    {
        server_.setConnectionCallback(
            std::bind(&TimeServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&TimeServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("TimeServer - %s -> %s is UP.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());

            time_t now = ::time(NULL);
            int32_t be32 = htobe32(static_cast<int32_t>(now));
            std::string buf(reinterpret_cast<char*>(&be32), sizeof(be32));
            Timestamp ts(static_cast<uint64_t>(now));
            LOG_INFO("server time = %s\n", ts.toString().c_str());
            // std::string buf("hello world!");
            conn->send(buf);
            conn->shutdown();

        } else {
            LOG_INFO("TimeServer - %s -> %s is DOWN.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg(buf->retrieveAllAsString());

        LOG_INFO("%s discards message %d bytes received at %s\n",
            conn->name().c_str(), (int)msg.size(), time.toString().c_str());

        conn->send(msg);
    }

    TcpServer server_;
};

int main()
{
    LOG_INFO("pid = %d\n", getpid());

    EventLoop loop;
    InetAddress listenAddr(7890, "192.168.110.132");
    TimeServer server(&loop, listenAddr);

    server.start();
    loop.loop();

    return 0;
}