#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <functional>
#include <mymuduo/TcpServer.h>
#include <string>
#pragma once

class HttpRequest;
class HttpResponse;

// 一个简单的HTTP服务器，用于报告状态，只提供了最小功能
// 能够与HTTPClient和Web浏览器通信
class HttpServer {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& name,
        TcpServer::Option option = TcpServer::kNoReusePort);

    EventLoop* getLoop() const { return server_.getLoop(); }

    void setHttpCallback(const HttpCallback& cb)
    {
        httpCallback_ = cb;
    }

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start();

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr&, const HttpRequest&);

    TcpServer server_;
    HttpCallback httpCallback_;
};

#endif