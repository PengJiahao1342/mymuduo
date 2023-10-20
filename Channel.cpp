#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <memory>
#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 文件描述符可读 | 有tcp紧急的数据可读
const int Channel::kWriteEvent = EPOLLOUT;

// 构造函数初始化
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel() { }

// Channel::tie在什么时候调用？
// TcpConnection建立连接时调用，保证TcpConnection没有被释放，才能进行相应操作
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj; // 赋值给弱智能指针
    tied_ = true;
}

//
// 当改变Channel所表示的fd的events事件后，
// update负责在Poller中更新fd相应的事件，epoll_ctl
//
void Channel::update()
{
    // 通过Channel所属的EventLoop，调用Poller相应的方法，注册fd的events

    loop_->updateChannel(this);
}

// 在Channel所属的EventLoop中，删除当前的Channel
void Channel::remove()
{
    loop_->removeChannel(this);
}

// fd得到Poller通知以后，处理事件，采用回调函数
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) {
        // 尝试把弱智能指针提升为强智能指针，提升成功则指针所指对象存活
        // 提升失败表示指针所指对象已经释放，提升失败不进行任何调用
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

// 根据Poller通知Channel具体发生的事件执行回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel handelEvent revents: %d\n", revents_);

    // 对端关闭并且没有数据可读时关闭连接 -- 对端关闭时可能还有一些剩余数据可读，读完后关闭
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}