#include "EPollPoller.h"
#include "Channel.h"
#include "Logger.h"

#include <asm-generic/errno-base.h>
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

// 状态指示
const int kNew = -1; // Channel还未添加到Poller中 Channel成员index_=-1
const int kAdded = 1; // Channel已经添加到Poller中
const int kDeleted = 2; // Channel已经从Poller中删除

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC)) // epoll_create1子进程继承时会把父进程的fd关闭
    , events_(kInitEventListSize)
{
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error: %d\n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

// Channel update remove -> EvenrLoop updateChannel removeChannel -> Poller updateChannel removeChannel
void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_INFO("func = %s => fd = %d, events= %d, index = %d\n",
        __FUNCTION__, channel->fd(), channel->events(), channel->index());

    if (index == kNew || index == kDeleted) {
        // 新的Channel和已删除的Channel都可以添加到Poller中
        int fd = channel->fd();
        if (index == kNew) {
            channels_[fd] = channel; // 往Poller的map中添加键值对
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    } else { // index==kAdded 已经在Poller上注册过 EPOLL_CTL_MOD/DEL

        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除Channel
void EPollPoller::removeChannel(Channel* channel)
{
    LOG_DEBUG("func = %s => fd = %d\n", __FUNCTION__, channel->fd());

    // 从Poller的ChannelMap中移除
    int fd = channel->fd();
    channels_.erase(fd);

    // 如果还是epoll监听状态，还要从epoll中移除
    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 更新Channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            // 删除选项没有删掉为一般错误
            LOG_ERROR("epoll_ctl del error: %d\n", errno);
        } else {
            // 添加或修改事件为致命错误，会影响后续监听，要退出进程
            LOG_FATAL("epoll_ctl add/mod error: %d\n", errno);
        }
    }
}

// epoll_wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    // 先使用INFO输出日志，这里频繁调用，要改成DEBUG
    LOG_DEBUG("func = %s => fd total conut: %zu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
        static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;

    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);

        // events_数组满了，就需要扩容，有可能发生的事件比数组大小还多
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    } else { // 有错误发生
        if (savedErrno != EINTR) {
            errno = savedErrno;
            LOG_ERROR("EPollPoller::poll() error\n");
        }
    }
    return now;
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    // poll()中的epoll_wait把发生的event放到数组events_中，遍历events_即可
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // 取出发生事件的Channel
        channel->set_revents(events_[i].events); // 给Channel设置具体发生的事件
        activeChannels->push_back(channel); // 向ChannelList中添加活跃连接，传给EventLoop，EventLoop针对这些发生事件的Channel进行处理
    }
}