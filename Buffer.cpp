#include "Buffer.h"

#include <cerrno>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend; // 一次准备读取字节数可以写在头部
const size_t Buffer::kInitialSize; // 缓冲区大小

// 从fd中读取数据 Poller工作在LT模式，能保证数据读完
// Buffer缓冲区有大小，但是从fd上读取数据时并不知道TCP数据最终的大小
// 这里使用readv系统调用，Buffer缓冲区+函数栈上的空间进行读取
ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65536]; // 栈上内存空间，64K

    struct iovec vec[2];

    const size_t writable = writableBytes(); // writable是Buffer缓冲区剩余可写空间大小

    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // 最大使用64k的缓冲区
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno; // 保存错误号
    } else if (n <= writable) { // Buffer缓冲区就够用
        writeIndex_ += n;
    } else { // extrabuf中也有数据，说明Buffer缓冲区满了
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 从缓冲区末尾写n-writable大小的数据，需要扩容
    }

    return n;
}

// 通过fd发送数据---把Buffer中readable缓冲区中的数据通过fd发送
ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *savedErrno = errno;
    }
    return n;
}