#include <mymuduo/TcpServer.h>
#include <functional>
#include <string>
#include <unistd.h>

// 丢弃所有收到的数据，只关注消息/数据到达
class DiscardServer {
public:
    DiscardServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "DiscardServer")
    {
        server_.setConnectionCallback(
            std::bind(&DiscardServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&DiscardServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("DiscardServer - %s -> %s is UP.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
        } else {
            LOG_INFO("DiscardServer - %s -> %s is DOWN.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg(buf->retrieveAllAsString());

        LOG_INFO("%s discards message[%s] %d bytes received at %s\n",
            conn->name().c_str(), msg.c_str(), (int)msg.size(), time.toString().c_str());

        std::string test("hello\n");
        LOG_INFO("%s message %d bytes\n", test.c_str(), (int)test.size());
    }

    TcpServer server_;
};

int main()
{
    LOG_INFO("pid = %d\n", getpid());

    EventLoop loop;
    InetAddress listenAddr(7890, "192.168.110.132");
    DiscardServer server(&loop, listenAddr);

    server.start();
    loop.loop();

    return 0;
}