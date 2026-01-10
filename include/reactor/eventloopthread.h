#pragma once

#include "noncopyable.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

namespace reactor 
{

class EventLoop;

// EventLoopThread 封装"线程 + EventLoop"
// 职责：
// 1. 创建新线程
// 2. 在新线程中运行EventLoop::loop()
// 3. 提供同步机制：确保EventLoop创建完成后才返回
//
// 使用示例：
//   EventLoopThread loopThread;
//   EventLoop* loop = loopThread.startLoop();  // 阻塞直到线程启动
//   loop->runInLoop(callback);                 // 跨线程调用

class EventLoopThread : private NonCopyable
{
public:
    explicit EventLoopThread();
    ~EventLoopThread();

    // 启动线程，返回EventLoop指针
    // 会阻塞直到EventLoop创建完成
    EventLoop* startLoop();

private:
    void threadFunc();

private:
    EventLoop* m_loop;
    std::thread m_thread;
    std::mutex m_mtx;
    std::condition_variable m_cond;
};

}