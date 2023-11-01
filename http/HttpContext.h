#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

#pragma once
#include "HttpRequest.h"

class Buffer;
class Timestamp;

// HTTP请求解析器，对于不同内容的解析方法不同
class HttpContext {
public:
    enum HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext()
        : state_(kExpectRequestLine)
    {
    }

    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    bool gotAll() const { return state_ == kGotAll; }

    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    HttpRequest& request() { return request_; }
    const HttpRequest& request() const { return request_; }

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
};

#endif