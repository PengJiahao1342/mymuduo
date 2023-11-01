#include "TimerQueue.h"
#include "EPollPoller.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Timer.h"
#include "TimerId.h"
#include "Timestamp.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <netinet/in.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

static int createTimerfd()
{
    // CLOCK_MONOTONIC 以固定速率运行，不进行调整和复位，flag创建的是非阻塞的timerfd
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_FATAL("Failed int timerfd_create\n");
    }
    return timerfd;
}

// 用于读取文件描述符，有超时时间会发送一个字节的数据，成功读取后可以开始处理定时函数
static void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_INFO("TimerQueue::handleRead() %lu at %s\n", howmany, now.toString().c_str());
    if (n != sizeof howmany) {
        LOG_ERROR("TimerQueue::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

// when传入的是超时时间 返回距离当前时间的timespec
static struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();

    if (microseconds < 100) {
        microseconds = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

// 用于修改timerfd的超时时间
static void resetTimerfd(int timerfd, Timestamp expiration)
{
    // timerfd_settime()用于启动或者关闭由fd指定的定时器
    struct itimerspec newValue;
    struct itimerspec oldValue;
    ::memset(&newValue, 0, sizeof newValue);
    ::memset(&oldValue, 0, sizeof oldValue);
    // it_value第一次超时时间 it_interval表示之后每隔多长时间超时
    newValue.it_value = howMuchTimeFromNow(expiration);

    // 1代表设置绝对时间 0代表相对时间--相对当前时间
    // newValue.it_value非0则启动定时器，如果newValue.it_interval为0则启动一次定时器
    // 之前设置的超时时间返回到oldValue
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        LOG_ERROR("timerfd_settime()\n");
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(createTimerfd())
    , timerfdChannel_(loop, timerfd_) // timerfd_封装成Channel
    , timers_()
    , callingExpiredTimers_(false)
{
    // 绑定timerfdChannel_的回调函数是TimerQueue::handleRead
    timerfdChannel_.setReadCallBack(std::bind(&TimerQueue::handleRead, this));
    // 因为我们只会对timerfd的读事件感兴趣，将超时的timer读取出来，所以开启读事件监听并且添加到Poller中
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    for (const Entry& timer : timers_) {
        delete timer.second;
    }
}

// 向TimerQueue队列中添加一个timer
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInloop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    bool earliestChanged = insert(timer);
    if (earliestChanged) {
        // 如果新加入的定时器是最早执行的，需要修改timerfd的定时时间
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInloop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    // 通过timerId构建ActiveTimer对象，并在ActiveTimerSet中查找
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);

    if (it != activeTimers_.end()) {
        timers_.erase(Entry(it->first->expiration(), it->first));
        delete it->first;
        activeTimers_.erase(it);
    } else if (callingExpiredTimers_) {
        // 如果正在调用回调函数，需要先加入待删除集合中
        cancelingTimers_.insert(timer);
    }
}

// timerfd有读事件到来回调这个函数
// 处理timerfd，并运行超时的定时器超时处理函数
void TimerQueue::handleRead()
{
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    // 获取超时的timer
    std::vector<Entry> expired = getExpired(now);

    // 遍历集合对超时的timer运行run，=>Timer::callback_()
    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    for (const Entry& it : expired) {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    // 重置上面调用timer的定时任务
    // 循环执行还要设置下一次执行时间
    reset(expired, now);
}

// 获取所有超时的定时器
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    // 创建一个pair对象，哨兵，UINTPTR_MAX是一个足够大的整数，确保可以转换为Timer*指针
    // 用于比较是否超时，当前时间之前的定时器都超时了
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    // lower_bound返回set集合中第一个不小于sentry的元素，默认比较第一个值，即Timestamp
    // end之前的值都小于now，说明已经超时
    TimerList::iterator end = timers_.lower_bound(sentry);

    // 超时的定时器都拷贝到expired中
    std::copy(timers_.begin(), end, std::back_inserter(expired));

    // 删除timers_和activeTimers_中与超时定时器相关的部分
    timers_.erase(timers_.begin(), end);
    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        activeTimers_.erase(timer);
    }

    // 返回超时定时器的vector
    return expired;
}

bool TimerQueue::insert(Timer* timer)
{
    bool earliestChanged = false;
    // 获取超时时间
    Timestamp when = timer->expiration();

    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) {
        // 如果set集合为空或者当前的超时时间是最小的
        // 就把标志位改为true，说明这个定时器是最早超时的
        earliestChanged = true;
    }

    // 不管是不是最早执行的，都要加入到timers_和activeTimers_中
    timers_.insert(Entry(when, timer));
    activeTimers_.insert(ActiveTimer(timer, timer->sequence()));

    return earliestChanged;
}

// 因为一个定时器Timer可能设置了repeat_和interval_
// 需要重新设置下一次timer的执行时间
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat()
            && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            // 有设置重复执行的话需要修改超时时间后 再加入到定时器集合中
            it.second->restart(now);
            insert(it.second);
        } else {
            // 否则就释放
            delete it.second;
        }
    }

    if (!timers_.empty()) {
        // timers_非空，由于按超时时间从小到大排序，begin的超时时间最小
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        // 重置timerfd_的超时时间
        resetTimerfd(timerfd_, nextExpire);
    }
}