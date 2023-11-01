#include "HttpContext.h"
#include "HttpRequest.h"

#include <algorithm>
#include <mymuduo/Buffer.h>

bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
    bool ok = true;
    bool hasMore = true;

    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            // 解析请求行
            const char* crlf = buf->findCRLF(); // 找第一个\r\n
            if (crlf) { // 第一个\r\n之前的数据是请求行
                ok = processRequestLine(buf->peek(), crlf);
                if (ok) {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieve(crlf + 2 - buf->peek());

                    // 解析请求行完成，接下来解析请求头
                    state_ = kExpectHeaders;
                } else {
                    hasMore = false; // 解析出错，停止解析
                }
            }else {
                hasMore = false; // 解析出错，停止解析
            }
        }else if (state_ == kExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                // 请求头以:分隔key-value
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {
                    // 请求头的key-value添加到map中
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else {
                    // 没有:，空行，空行是请求头的结束标志
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieve(crlf + 2 - buf->peek());
            }else {
                hasMore = false;
            }
        }else if (state_ == kExpectBody) {
            
        }
    }
    return ok;
}

bool HttpContext::processRequestLine(const char* begin, const char* end)
{
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');

    // 请求行格式---GET /hello.txt HTTP/1.1
    // 请求方法 URL 协议版本\r\n 中间以空格分隔
    if (space != end && request_.setMethod(start, space)) {
        // 成功获取请求方法，继续查找URL
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end) {
            const char* question = std::find(start, space, '?');
            if (question != space) {
                request_.setPath(start, question); // ？前是路径
                request_.setQuery(question, space);
            } else {
                request_.setPath(start, space); // 整个URL就是路径
            }

            // 成功获取URL，继续获取协议版本
            start = space + 1;
            // 检查协议字符是否正确
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed) {
                if (*(end - 1) == '1') {
                    request_.setVersion(HttpRequest::kHttp11);
                } else if (*(end - 1) == '0') {
                    request_.setVersion(HttpRequest::kHttp10);
                } else {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}
