#include "Poller.h"
#include "EPollPoller.h"

#include <cstdlib>

// 这个函数是虚基类的静态方法，需要引用子类的头文件，所以不放在Poller.cpp中实现
// EventLoop通过这个接口获取默认的IO复用的实例
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if (::getenv("MUDUO_USE_POLL")) {
        return nullptr; // 生成poll的实例
    } else {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}