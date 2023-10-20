#ifndef BUFFER_H
#define BUFFER_H

#include <sys/types.h>
#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

//
// +-------------------+----------------+----------------+
// | prependable bytes | readable bytes | writable bytes |
// |                   |                |                |
// +-------------------+----------------+----------------+
// |                   |                |                |
// 0       <=     readerIndex   <=   writerIndex  <=    size
// readable bytes保存要向外写的数据 writable bytes保存外部要读进来的数据
//

// 网络库底层缓冲区定义
class Buffer {
public:
    static const size_t kCheapPrepend = 8; // 一次准备读取字节数可以写在头部
    static const size_t kInitialSize = 1024; // 缓冲区大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writeIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writeIndex_ - readerIndex_; }

    size_t writalbeBytes() const { return buffer_.size() - writeIndex_; }

    size_t prependableBytes() const { return readerIndex_; }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_; }

    void retrieve(size_t len)
    {
        if (len < readableBytes()) {
            readerIndex_ += len; // 只读取了一部分，后面readerIndex_ += len到writeIndex的数据还没读
        } else { // len == readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writeIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据转成string
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句已经把缓冲区中可读数据提取出来，这里对缓冲区进行复位
        return result;
    }

    // 可写缓冲区大小buffer_.size() - writeIndex_ 要跟写数据长度len比较，空间不足需要扩容
    void ensureWritableBytes(size_t len)
    {
        if (writalbeBytes() < len) {
            makeSpace(len); // 扩容函数
        }
    }

    // 把data数据添加到writer缓冲区
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writeIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writeIndex_;
    }
    const char* beginWrite() const
    {
        return begin() + writeIndex_;
    }

    // 从fd中读取数据
    ssize_t readFd(int fd, int* savedErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* savedErrno);

private:
    // 返回buffer_的首地址
    char* begin()
    {
        return &*buffer_.begin(); // 先对iterator解引用再取地址
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }

    // vector扩容函数
    void makeSpace(size_t len)
    {
        // prependableBytes返回readerIndex_，如果读取一部分数据，reader缓冲区能空出一部分空间写数据
        // writer缓冲区+空出的reader缓冲区不足以写数据就需要扩容
        if (writalbeBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writeIndex_ + len);
        } else {
            // 如果空间够，把未读的数据拷贝到读缓冲区头部，将已读空间和writer缓冲区连起来
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                begin() + writeIndex_,
                begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writeIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_; // vector方便扩容
    size_t readerIndex_;
    size_t writeIndex_;
};

#endif