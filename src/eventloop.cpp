#include "reactor/eventloop.h"
#include "reactor/channel.h"
#include "reactor/poller.h"
#include "reactor/timerqueue.h"
#include <cassert>
#include <iostream>
#include <sys/eventfd.h>

namespace reactor
{
// thread_local 变量：每个线程独有的 EventLoop 指针
// 用于实现 "One Loop Per Thread"
thread_local EventLoop* loopInThisThread = nullptr;

//epoll wait timeout
constexpr int kPollTimeoutMs = 10000; // 10秒

static int createEventFd()
{
    int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if(fd < 0)
    {
        std::cerr << "Failed in eventfd" << std::endl;
        abort();
    }
    return fd;
}

EventLoop::EventLoop()
    :m_isLooping(false),
     m_quit(false),
     m_callingPendingFunctors(false),
     m_threadId(tid()),
     m_poller(std::make_unique<Poller>()),
     m_timerQueue(std::make_unique<TimerQueue>(this)), // 初始化 TimerQueue
     m_wakeupFd(createEventFd()),
     m_wakeupChannle(std::make_unique<Channel>(this, m_wakeupFd))
{
    std::cout << "EventLoop created " << this << " in thread " << m_threadId << std::endl;

    // 检查当前线程是否已经有 EventLoop
    if(loopInThisThread)
    {
        abortNotInLoopThread();
    }
    else
    {
        loopInThisThread = this; // 设置当前线程的 EventLoop
    }   

    //设置wakepfd
    m_wakeupChannle->setReadCallback(std::bind(&EventLoop::handleReadForWakeupFd, this));
    m_wakeupChannle->enableReading();
}

EventLoop::~EventLoop()
{
    std::cout << "EventLoop destroyed " << this << " in thread " << m_threadId << std::endl;

    m_wakeupChannle->disableAll();
    m_wakeupChannle->remove();
    close(m_wakeupFd);
    assert(!m_isLooping);
    loopInThisThread = nullptr; // 清除当前线程的 EventLoop
}

void EventLoop::loop()
{
    assert(!m_isLooping);
    assertInLoopThread();

    m_isLooping = true;
    m_quit = false;

    while (!m_quit)
    {
        m_activeChannels = m_poller->poll(kPollTimeoutMs);

        for (Channel* channel : m_activeChannels)
        {
            channel->handleEvent(); // 处理每个活跃的 Channel 事件
        }

        // 处理pending任务
        doPendingFunctors();
    }

    std::cout << "EventLoop " << this << " stop looping" << std::endl;
    m_isLooping = false;
}

void EventLoop::quit()
{
    m_quit = true;
    if(!isInLoopThread()) wakeup();
}

void EventLoop::updateChannel(Channel* channel)
{
    assertInLoopThread();
    assert(channel->ownerLoop() == this);
    m_poller->updateChannel(channel); // 更新 Poller 中的 Channel
}

void EventLoop::removeChannel(Channel* channel)
{
    assertInLoopThread();
    assert(channel->ownerLoop() == this);
    m_poller->removeChannel(channel); // 从 Poller 中移除 Channel
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
    return loopInThisThread;
}

void EventLoop::abortNotInLoopThread()
{
    std::cerr << "EventLoop::abortNotInLoopThread() - EventLoop " << this
              << " was created in thread " << m_threadId
              << ", but called in thread " << tid() << std::endl;
    abort();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
    return m_timerQueue->addTimer(std::move(cb), time, 0.0); // 添加单次定时器
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb)); // 添加延时定时器
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return m_timerQueue->addTimer(std::move(cb), time, interval); // 添加重复定时器
}

void EventLoop::cancel(TimerId timerId)
{
    m_timerQueue->cancel(timerId); // 取消定时器
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb(); // 如果在 Loop 线程，直接执行
    }
    else
    {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_pendingFactors.push_back(std::move(cb));
    }
    // 情况1：在其他线程调用
    // → 必须唤醒，否则Loop可能阻塞在poll中

    // 情况2：在Loop线程，但正在执行pending任务
    // → 也要唤醒，因为当前循环的poll已经返回了
    //   如果不唤醒，新任务要等下次poll超时才执行
    if(!isInLoopThread() || m_callingPendingFunctors) wakeup();
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(m_wakeupFd, &one, sizeof one);
    if (n != sizeof one) 
    {
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::handleReadForWakeupFd()
{
    uint64_t one = 1;
    ssize_t n = read(m_wakeupFd, &one, sizeof one);
    if (n != sizeof one) 
    {
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    m_callingPendingFunctors = true;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        functors.swap(m_pendingFactors);
    }

    for(const Functor& f : functors) f();

    m_callingPendingFunctors = false;

}

}