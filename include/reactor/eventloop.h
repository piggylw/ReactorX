#pragma once

#include "noncopyable.h"
#include "currentthread.h"
#include "timerid.h"
#include "callbacks.h"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

namespace reactor {

class Channel;
class Poller;
class TimerQueue;
class Timestamp;

// EventLoop 是事件循环
// 职责：
// 1. 循环调用 Poller::poll() 获取活跃事件
// 2. 分发事件到各个 Channel
// 3. 执行pending任务（后续会用到）
//
// One Loop Per Thread 原则：
// - 一个线程只能有一个 EventLoop
// - EventLoop 的所有操作必须在创建它的线程执行

class EventLoop : private NonCopyable
{
public:
    explicit EventLoop();
    ~EventLoop();

    void loop(); // 启动事件循环
    void quit(); // 退出事件循环
    void updateChannel(Channel* channel); // 更新 Channel
    void removeChannel(Channel* channel); // 移除 Channel

    // 新增：在指定时间运行回调
    TimerId runAt(Timestamp time, TimerCallback cb);
    
    // 新增：在delay秒后运行回调
    TimerId runAfter(double delay, TimerCallback cb);
    
    // 新增：每隔interval秒运行回调
    TimerId runEvery(double interval, TimerCallback cb);
    
    // 新增：取消定时器
    void cancel(TimerId timerId);

    // 新增：在Loop线程执行回调（跨线程调用安全）
    // 如果在Loop线程调用，直接执行
    // 如果在其他线程调用，加入队列
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    void wakeup();

    // 判断当前线程是否是Loop线程
    bool isInLoopThread() const
    {
        return m_threadId == tid();
    }

    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }

    // 获取当前线程的EventLoop指针
    // 如果当前线程没有EventLoop，返回nullptr
    static EventLoop* getEventLoopOfCurrentThread();

private:
    void abortNotInLoopThread();
    void handleReadForWakeupFd(); //处理wakeupfd读事件
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic<bool> m_isLooping; // 是否正在循环中
    std::atomic<bool> m_quit; // 是否退出循环
    std::atomic<bool> m_callingPendingFunctors;
    const pid_t m_threadId; // 创建 EventLoop 的线程 ID
    std::unique_ptr<Poller> m_poller; // Poller 实例
    std::unique_ptr<TimerQueue> m_timerQueue; // TimerQueue 实例
    ChannelList m_activeChannels; // 活跃的 Channel 列表
    int m_wakeupFd; //eventfd
    std::unique_ptr<Channel> m_wakeupChannle;
    std::mutex m_mtx;
    std::vector<Functor> m_pendingFactors;
};

}