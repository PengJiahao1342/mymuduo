#ifndef CHANNEL_H
#define CHANNEL_H

#pragma once
#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>
#include <utility>

class EventLoop;

//
// 一个线程Thread有一个EventLoop，一个EventLoop有一个Poller监听一个ChannelList，Poller中可以监听很多Channel
// Channel具体可以理解为通道，
// 封装了sockfd和其感兴趣的event，比如EPOLLIN、EPOLLOUT事件
// 还绑定了poller返回的具体事件
//
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // fd得到Poller通知以后，处理事件，采用回调函数
    void handleEvent(Timestamp receiveTime);

    // 提供接口设置回调函数对象
    void setReadCallBack(ReadEventCallback cb) { readCallback_ = std::move(cb); } // cb是左值，用move转成右值后给函数对象赋值，减少内存开销
    void setWriteCallBack(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallBack(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallBack(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当Channel被手动remove之后，还在执行回调操作，通过成员变量的弱智能指针来监听
    void tie(const std::shared_ptr<void>&);

    // 提供接口查询有关成员变量
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // Poller监听到具体发生的事件后，通过这个接口设置revents_

    // 设置fd相应的事件状态，通过自定义的标识符
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:
    void update(); // epoll_ctl注册事件
    void handleEventWithGuard(Timestamp receiveTime);

    // 事件注册标识 通过events_与这几个变量比较来判断是否有相应事件注册
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; // 事件循环
    const int fd_; // socketfd, Poller监听的对象
    int events_; // 注册fd感兴趣的事件
    int revents_; // Poller返回的具体发生的事件
    int index_; // Channel添加到Poller中的状态指示 -1未添加 1已添加 2已删除

    std::weak_ptr<void> tie_; // 通过一个弱智能指针来监听对象是否存在
    bool tied_;

    // Channel通道中能够获知fd最终发生的具体的事件revents
    // 所有Channel负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

#endif