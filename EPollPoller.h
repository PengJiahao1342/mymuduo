#ifndef EPOLLPOLLER_H
#define EPOLLPOLLER_H

#pragma once
#include "Poller.h"

#include <sys/epoll.h>
#include <vector>

class EventLoop;
class Channel;

//
// epoll的使用
// epoll_create -- EPollPoller(EventLoop* loop)
// epoll_ctl    add/mod/del -- updateChannel / removeChannel
// epoll_wait -- poll(int timeoutMs, ChannelList* activeChannels)
//
class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override; // override覆盖虚函数

    // 重写基类Poller的虚方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    // 更新Channel通道
    void update(int operation, Channel* channel);

    using EventList = std::vector<struct epoll_event>;

    int epollfd_;
    EventList events_;
};

#endif