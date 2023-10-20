#ifndef POLLER_H
#define POLLER_H

#pragma once
#include "Timestamp.h"
#include "noncopyable.h"

#include <unordered_map>
#include <vector>

class EventLoop;
class Channel;

// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    // 对IO复用保留统一接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数channel是否在当前Poller中
    bool hasChannel(Channel* channel) const;

    // EventLoop通过这个接口获取默认的IO复用的实例 类似与单例模式的接口
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // 不需要对Channel进行排序，用unordered_map效率高，内置的是哈希表
    // map的key是sockfd，value是sockfd所属Channel
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_; // 定义Poller所属的事件循环EventLoop
};

#endif