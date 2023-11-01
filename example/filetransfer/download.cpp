#include <cstddef>
#include <cstdio>
#include <functional>
#include <mymuduo/Callbacks.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/InetAddress.h>
#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>
#include <string>
#include <unistd.h>

// 一次性把文件读入内存，一次性调用send发送完毕，
// 文件过大占用很大内存

const char* g_file = nullptr;

std::string readFile(const char* filename)
{
    std::string content;
    FILE* fp = ::fopen(filename, "rb");

    // 文件越大内存消耗越多
    if (fp) {
        const int kBufSize = 1024 * 1024; // 1MB
        char iobuf[kBufSize];
        ::setbuffer(fp, iobuf, sizeof iobuf);

        char buf[kBufSize];
        size_t nread = 0;
        while ((nread = ::fread(buf, 1, sizeof buf, fp)) > 0) {
            content.append(buf, nread);
        }
        ::fclose(fp);
    }
    return content;
}

void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
    LOG_INFO("HighWaterMark %zu", len);
}

void onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        LOG_INFO("FileServer - %s -> %s is UP.\n",
            conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());

        LOG_INFO("FileServer - sending file %s to %s\n", g_file, conn->peerAddress().toIpPort().c_str());

        conn->setHighWaterMarkCallback(std::bind(&onHighWaterMark, std::placeholders::_1, 64 * 1024));

        // 考虑到文件可能被修改，每次建立连接都把文件读一遍
        std::string fileContent = readFile(g_file);
        conn->send(fileContent);
        conn->shutdown();
        LOG_INFO("FIleServer - done\n");

    } else {
        LOG_INFO("FileServer - %s -> %s is DOWN.\n",
            conn->peerAddress().toIpPort().c_str(), conn->localAddress().toIpPort().c_str());
    }
}

int main(int argc, char* argv[])
{
    LOG_INFO("pid = %d\n", getpid());

    if (argc > 1) {
        g_file = argv[1];

        EventLoop loop;
        InetAddress listenAddr(7890, "192.168.110.132");
        TcpServer server(&loop, listenAddr, "FileServer");
        server.setConnectionCallback(onConnection);
        server.start();
        loop.loop();
    } else {
        fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
    }

    return 0;
}