#include <functional>
#include <mymuduo/TcpServer.h>
#include <string>
#include <unistd.h>

// 发送完当前时间后，服务器主动断开连接，只需关注连接已建立事件
class DaytimeServer {
public:
    DaytimeServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "DaytimeServer")
    {
        server_.setConnectionCallback(
            std::bind(&DaytimeServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&DaytimeServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("DaytimeServer - %s -> %s is UP.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());

            conn->send(Timestamp::now().toString() + "\n");
            conn->shutdown();

        } else {
            LOG_INFO("DaytimeServer - %s -> %s is DOWN.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg(buf->retrieveAllAsString());

        LOG_INFO("%s discards message %d bytes received at %s\n",
            conn->name().c_str(), (int)msg.size(), time.toString().c_str());
    }

    TcpServer server_;
};

int main()
{
    LOG_INFO("pid = %d\n", getpid());

    EventLoop loop;
    InetAddress listenAddr(7890, "192.168.110.132");
    DaytimeServer server(&loop, listenAddr);

    server.start();
    loop.loop();

    return 0;
}