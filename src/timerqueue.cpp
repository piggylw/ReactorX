#include "reactor/timerqueue.h"
#include "reactor/eventloop.h"
#include "reactor/timer.h"
#include "reactor/timerid.h"
#include "reactor/channel.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <iostream>

namespace reactor
{
namespace details
{

int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        std::cerr << "Failed to create timerfd: " << strerror(errno) << std::endl;
        abort();
    }
    return timerfd;
}

// 计算超时时间（从现在到when）
struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microSeconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if(microSeconds < 100)
        microSeconds = 100; // 最小超时时间为100微秒
    
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSeconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microSeconds % Timestamp::kMicroSecondsPerSecond) * 1000); // 转换为纳秒
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    std::cout << "TimerQueue::handleRead() " << howmany << " at "
              << now.toString() << std::endl;
    if (n != sizeof howmany) {
        std::cerr << "TimerQueue::handleRead() reads " << n << " bytes instead of 8" << std::endl;
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    struct itimerspec newValue;
    struct itimerspec oldValue;
    std::memset(&newValue, 0, sizeof newValue);
    std::memset(&oldValue, 0, sizeof oldValue);
    // 注意：我们只设置 it_value，不设置 it_interval
    // 因为 TimerQueue 自己管理重复逻辑，不依赖 timerfd 的重复功能
    newValue.it_value = howMuchTimeFromNow(expiration);
    
    timerfd_settime(timerfd, 0, &newValue, &oldValue);

}


}// namespace details

TimerQueue::TimerQueue(EventLoop* loop)
    : m_loop(loop),
      m_timerfd(details::createTimerfd()),
      m_timers(),
      m_timerfdChannel(std::make_unique<Channel>(loop, m_timerfd)),
      m_cancellingTimers()
{
    m_timerfdChannel->setReadCallback(std::bind(&TimerQueue::handleRead, this));
    m_timerfdChannel->enableReading();
}

TimerQueue::~TimerQueue()
{
    m_timerfdChannel->disableAll();
    m_timerfdChannel->remove();
    close(m_timerfd);

    for(const Entry& entry : m_timers)
    {
        delete entry.second; // 删除定时器对象
    }
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    m_loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
    m_loop->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    m_loop->assertInLoopThread();
    Entry entry(timerId.timer()->expiration(), timerId.timer());
    
    auto it = m_timers.find(entry);
    if(it != m_timers.end())
    {
        m_timers.erase(it);
        delete timerId.timer();
    }
    else
    {
        //如果当前线程定时器的回调函数是取消其他定时器，需要m_cancellingTimers解决冲突
        m_cancellingTimers.insert(timerId.timer());
    }

}

void TimerQueue::handleRead()
{
    m_loop->assertInLoopThread();
    Timestamp now(Timestamp::now());
    details::readTimerfd(m_timerfd, now);

    std::vector<Entry> expired = getExpired(now);
    for(const Entry& it : expired)
    {
        if(m_cancellingTimers.find(it.second) == m_cancellingTimers.end())
        {
            it.second->run();
        }
    }

    reset(expired, now);
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    for(const Entry& it : expired)
    {
        if(it.second->repeat() && m_cancellingTimers.find(it.second) == m_cancellingTimers.end())
        {
            it.second->restart(now);
            insert(it.second);
        }
        else
            delete it.second;
    }

    m_cancellingTimers.clear();
    // 如果还有定时器，重置timerfd
    if(!m_timers.empty())
    {
        Timestamp nextExpire = m_timers.begin()->first;
        if(nextExpire.valid()) details::resetTimerfd(m_timerfd, nextExpire);
    }

}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<TimerQueue::Entry> expired;

    Entry entry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    // lower_bound: 返回第一个 >= entry 的迭代器
    //所有 < now 的定时器都在 [begin, end) 区间
    auto end = m_timers.lower_bound(entry);
    assert(end == m_timers.end() || now < end->first);

    std::copy(m_timers.begin(), end, back_inserter(expired));
    m_timers.erase(m_timers.begin(), end);

    return expired;
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    m_loop->assertInLoopThread();

    bool isEarliestTimer = insert(timer);
    if(isEarliestTimer) details::resetTimerfd(m_timerfd, timer->expiration());
}

bool TimerQueue::insert(Timer* timer)
{
    assert(m_timers.size() < 100000);
    
    bool isEarliestTimer = false;   //是否是最早的定时器
    Timestamp when = timer->expiration();
    auto it = m_timers.begin();
    if(it == m_timers.end() || when < it->first)
        isEarliestTimer = true;

    auto result = m_timers.insert(Entry(when, timer));
    assert(result.second);
    return isEarliestTimer;
}

}// namespace reactor