#pragma once

#include <cstdint>

namespace reactor
{

class Timer;

// TimerId 是定时器的句柄（类似文件描述符）
// 用途：
// 1. 作为 runAfter/runEvery 的返回值
// 2. 传递给 cancel() 取消定时器
//
// 设计：
// - Timer* + sequence 的组合确保唯一性
// - 即使Timer对象被删除后指针被重用，sequence也不同
class TimerId
{
public:
    TimerId() : m_timer(nullptr), m_sequence(0) {}
    TimerId(Timer* timer, int64_t sequence)
        : m_timer(timer), m_sequence(sequence) {}
        
private:
    Timer* m_timer; // 指向定时器对象的指针
    int64_t m_sequence; // 定时器序列号，用于唯一标识
};

}