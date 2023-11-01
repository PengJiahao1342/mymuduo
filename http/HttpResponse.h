#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <string>
#include <unordered_map>
#pragma once

class Buffer;

// 用于创建服务器响应报文
class HttpResponse {
public:
    enum HttpStatusCode { // 响应状态
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k404NotFound = 404,
    };

    explicit HttpResponse(bool close)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {
    }

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& message) { statusMessage_ = message; }
    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string contentType)
    {
        addHeader("Content-Type", contentType);
    }
    void addHeader(const std::string& key, const std::string& value)
    {
        headers_[key] = value;
    }

    void setBody(const std::string& body) { body_ = body; }

    // 构造响应报文
    void appendToBuffer(Buffer* output) const;

private:
    std::unordered_map<std::string, std::string> headers_; // 消息报头map
    HttpStatusCode statusCode_; // 状态码
    std::string statusMessage_; // 响应状态
    std::string body_; // 响应体
    bool closeConnection_;
};

#endif