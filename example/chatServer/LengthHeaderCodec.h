#ifndef LENGTHHEADERCODEC_H
#define LENGTHHEADERCODEC_H

#include <cstdint>
#include <unistd.h>
#pragma once
#include "mymuduo/TcpConnection.h"
#include <mymuduo/Logger.h>

#include <cstddef>
#include <endian.h>
#include <functional>
#include <string>

//
// LengthHeaderCodec这个类用于给消息头部添加一个长度字段
// 显示当前消息的字节数，解决TCP分包问题
//

class LengthHeaderCodec : noncopyable {
public:
    using StringMessageCallback = std::function<void(const TcpConnectionPtr&,
        const std::string& message, Timestamp)>;

    explicit LengthHeaderCodec(const StringMessageCallback& cb)
        : messageCallback_(cb)
    {
    }

    void send(TcpConnectionPtr conn, const std::string msg)
    {
        Buffer buf;
        buf.retrieveAll();
        // buf缓冲区中添加msg
        buf.append(msg.c_str(), static_cast<size_t>(msg.size()));

        int32_t len = static_cast<int32_t>(msg.size());
        // int readable = buf.readableBytes();
        // int writable = buf.writableBytes();
        // LOG_INFO("readable: %d, wirtable:%d\n", readable, writable);
        // LOG_INFO("Codec send length %d\n", len); // *****不加这个日志len就输出不对*****
        int32_t be32 = htobe32(len); // 把len转换为大端字节序
        // 在readable区域前添加字节大小 也就添加到了msg前面
        // prependable预留了8字节的空间
        buf.prepend(&be32, sizeof be32);
        // readable = buf.readableBytes();
        // writable = buf.writableBytes();
        // LOG_INFO("readable: %d, wirtable:%d\n", readable, writable);

        std::string newmsg = buf.retrieveAllAsString();
        conn->send(newmsg);
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
    {
        while (buf->readableBytes() >= kHeaderLen) { // kHeaderLen = 4
            // 循环读取直到没有一条完整的消息，k
            // HeaderLen这段字节存放的是消息长度，有消息的话可读区域会大于kHeaderLen
            const void* data = buf->peek();
            // 在send中保存的是大端字节序
            // 这里在某些不支持非对齐内存访问的体系结构上会造成 SIGBUS core dump
            int32_t be32 = *static_cast<const int32_t*>(data);
            // 转换成主机字节序就是消息长度
            const int32_t len = be32toh(static_cast<uint32_t>(be32));

            // 判断len的大小考虑该消息是否正确接收
            if (len > 65535 || len < 0) {
                LOG_ERROR("Invalid length %d\n", len);
                conn->shutdown(); // 关闭连接
                break;
            } else if (buf->readableBytes() >= len + kHeaderLen) {
                LOG_INFO("Codec receive length %d\n", len);
                buf->retrieve(kHeaderLen); // 移动readIndex到消息开头
                std::string message(buf->peek(), len);
                messageCallback_(conn, message, receiveTime);
                // 已经处理完可读区域的这条消息，重置，接着循环处理直到缓冲区没有完整消息
                buf->retrieve(len);
            } else {
                break;
            }
        }
    }

private:
    StringMessageCallback messageCallback_;
    const static size_t kHeaderLen = sizeof(int32_t);
};

#endif