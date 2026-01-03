#pragma once

#include "timestamp.h"
#include "callbacks.h"
#include "noncopyable.h"
#include <atomic>

namespace reactor
{
// Timer 封装单个定时器
// 职责：
// 1. 存储到期时间和回调函数
// 2. 支持重复定时器（interval > 0）
// 3. 提供序列号用于唯一标识
class Timer : private NonCopyable
{
public:
    explicit Timer(TimerCallback cb, Timestamp expiration, double interval)
        : m_callback(std::move(cb)),
          m_expiration(expiration),
          m_interval(interval),
          m_repeat(interval > 0.0),
          m_sequence(s_sequence.fetch_add(1)) {}

    void run() const
    {
        if (m_callback)
        {
            m_callback();
        }
    }

    Timestamp expiration() const { return m_expiration; }
    bool repeat() const { return m_repeat; }
    int64_t sequence() const { return m_sequence; }
    void restart(Timestamp now);

    static int64_t sequenceNumber() { return s_sequence.load(); }

private:
    const TimerCallback m_callback; // 定时器回调函数
    Timestamp m_expiration; // 到期时间
    const double m_interval; // 间隔时间，0表示单次定时器
    const bool m_repeat; // 是否重复
    const int64_t m_sequence; // 定时器序列号，用于唯一标识

    static std::atomic<int64_t> s_sequence; // 静态序列号生成器
};

}