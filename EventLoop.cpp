#include "EventLoop.h"
#include "Channel.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Poller.h"

#include <cerrno>
#include <cstdint>
#include <functional>
#include <mutex>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

// 防止一个线程创建多个EventLoop
__thread EventLoop* t_loopInThisThread = nullptr;

// 默认IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 调用eventfd创建wakeupfd，用来notify唤醒subReactor处理新来的Channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error: %d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_)) // 相当于每个EventLoop都监听自己的wakeupFd_
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    } else {
        t_loopInThisThread = this;
    }

    // 设置wakeupFd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallBack(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的EPOIN读事件，mainReactor通过向wakeupChannel写数据来唤醒subReactor
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    // 只需关闭wakeupFd，其他new的对象通过智能指针释放
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// wakeup的唤醒回调函数
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8\n", n);
    }
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_) {
        activeChannels_.clear();

        // poller调用poll将活跃事件保存到activeChannels_中
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        // 对每个活跃事件进行事件处理
        // 有两类，一类是client的fd，一类是wakeupFd
        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_);
        }

        //
        // 执行当前EventLoop事件循环处理的回调操作
        // mainReactor主要是accept接收连接并把fd注册到Channel中，具体的Channel通过subReactor执行
        // mainReactor事先注册一个回调cb(需要subReactor来执行)
        // 上面for循环wakeup唤醒subReactor后执行下面的方法，mainReactor注册的pendingFunctors_(需要告诉subReactor唤醒subReactor后要做什么事)
        //
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping\n", this);
    looping_ = false;
}

// 退出事件循环
// 1. loop在自己的线程中调用quit，直接把quit_置为true
// 2. 在非loop的线程中调用loop的quit，因为一开始在阻塞睡眠，需要先唤醒
void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread()) {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInloop(Functor cb)
{
    if (isInLoopThread()) {
        cb();
    } else { // 在非当前loop线程中执行cb，需要先唤醒loop所在线程，执行cb
        queueInloop(cb);
    }
}

// 把cb放入队列中，唤醒loop所在线程，执行cb
void EventLoop::queueInloop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // push_back拷贝构造 emplace_back直接构造
    }

    // 唤醒需要执行上面回调操作的loop线程
    // callingPendingFunctors_为true时，当前loop正在执行回调，但又有了新的回调，执行完当前回调后会在poll阻塞，需要唤醒
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

// 唤醒loop所在线程
// 向wakeupFd写一个数据，wakeupChannel就发生读事件，当前loop线程会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::wakeup() writes %zu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

// 处理回调函数
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // 先交换cb到局部变量并把pendingFunctors_置空，方便高并发时快速存取cb
    }

    for (const Functor& functor : functors) {
        functor(); // 执行当前loop需要执行的回调
    }

    callingPendingFunctors_ = false;
}