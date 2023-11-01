#include <cstddef>
#include <cstdio>
#include <functional>
#include <mymuduo/Buffer.h>
#include <mymuduo/Callbacks.h>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>
#include <mymuduo/Timestamp.h>

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <unistd.h>

// chargen只发送数据，且发送数据的速度不能快过客户端接收的速度
class ChargenServer {
public:
    ChargenServer(EventLoop* loop, const InetAddress& listenAddr, bool print = false)
        : server_(loop, listenAddr, "ChargenServer")
        , transferred_(0)
        , startTime_(Timestamp::now())
    {
        server_.setConnectionCallback(
            std::bind(&ChargenServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&ChargenServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setWriteCompleteCallback(
            std::bind(&ChargenServer::onWriteComplete, this, std::placeholders::_1));

        if (print) {
            loop->runEvery(3.0, std::bind(&ChargenServer::printThroughput, this));
        }

        std::string line;
        for (int i = 33; i < 127; ++i) {
            line.push_back(char(i));
        }
        line += line;

        for (size_t i = 0; i < 127 - 33; ++i) {
            message_ += line.substr(i, 72) + "\n";
        }
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            LOG_INFO("ChargenServer - %s -> %s is UP.\n",
                conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());

            conn->setTcpNoDelay(true);
            conn->send(message_);

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
    }

    void onWriteComplete(const TcpConnectionPtr& conn)
    {
        transferred_ += message_.size();
        conn->send(message_);
    }

    void printThroughput()
    {
        Timestamp endTime = Timestamp::now();

        double diff = static_cast<double>(endTime.microSecondsSinceEpoch() - startTime_.microSecondsSinceEpoch())
            / Timestamp::kMicroSecondsPerSecond;

        printf("%4.3f MiB/s\n", static_cast<double>(transferred_) / diff / 1024 / 1024);
        transferred_ = 0;
        startTime_ = endTime;
    }

    TcpServer server_;

    std::string message_;
    int64_t transferred_;
    Timestamp startTime_;
};

int main()
{
    LOG_INFO("pid = %d\n", getpid());

    EventLoop loop;
    InetAddress listenAddr(7890, "192.168.110.132");
    ChargenServer server(&loop, listenAddr, true);

    server.start();
    loop.loop();

    return 0;
}