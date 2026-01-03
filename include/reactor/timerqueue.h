#pragma once

#include "noncopyable.h"
#include "timestamp.h"
#include "callbacks.h"
#include <set>
#include <vector>
#include <memory>

namespace reactor 
{
class EventLoop;
class Timer;
class TimerId;
class Channel;

// TimerQueue 管理所有定时器
// 职责：
// 1. 维护定时器列表（按到期时间排序）
// 2. 管理 timerfd（Linux特性）
// 3. 到期时执行定时器回调
// 4. 支持定时器的添加和取消
//
// 实现细节：
// - 使用 std::set 存储定时器（红黑树，自动排序）
// - 使用 timerfd 将定时器转换为可epoll的文件描述符
// - 最近的定时器到期时，timerfd 变为可读，触发回调
class TimerQueue : private NonCopyable
{
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();
    
    // 添加定时器
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    // 取消定时器
    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerSet = std::set<Entry>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);
    void handleRead(); // 处理 timerfd 可读事件
    std::vector<Entry> getExpired(Timestamp now); // 获取到期的定时器
    void reset(const std::vector<Entry>& expired, Timestamp now); // 重置到期的定时器
    bool insert(Timer* timer); // 插入定时器到集合

    EventLoop* m_loop; // 所属的 EventLoop
    const int m_timerfd; // timerfd 文件描述符
    TimerSet m_timers; // 定时器集合，按到期时间排序
    std::unique_ptr<Channel> m_timerfdChannel; // timerfd 的 Channel
    std::set<Timer*> m_cancellingTimers; // 正在取消的定时器集合

};

}