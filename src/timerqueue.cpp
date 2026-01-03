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

}// namespace reactor