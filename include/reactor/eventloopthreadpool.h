#pragma once

#include "noncopyable.h"
#include <vector>
#include <memory>
#include <functional>

namespace reactor 
{

class EventLoop;
class EventLoopThread;

// EventLoopThreadPool 管理一组EventLoopThread
// 职责：
// 1. 创建和管理多个工作线程
// 2. 提供负载均衡（Round-Robin）
// 3. 统一启动和停止

//EventLoopThreadPool要和baseloop在一个线程中使用
// 使用示例：
//   EventLoop baseLoop;  // 主Loop（Acceptor）
//   EventLoopThreadPool pool(&baseLoop);
//   pool.setThreadNum(4);
//   pool.start();
//   
//   // 获取下一个Loop（自动负载均衡）
//   EventLoop* ioLoop = pool.getNextLoop();

class EventLoopThreadPool : private NonCopyable
{
public:
    explicit EventLoopThreadPool(EventLoop* baseloop);
    ~EventLoopThreadPool();

     // 设置线程数（必须在start()前调用）
    void setThreadNum(int nums) { m_threadNums = nums; }
    void start();
    EventLoop* getNextLoop();  //TODO实现负载均衡
    std::vector<EventLoop*> getAllLoops();
    bool started() const { return m_started; }

private:
    EventLoop* m_baseloop;
    bool m_started;
    int m_threadNums;
    int m_next;
    std::vector<std::unique_ptr<EventLoopThread>> m_pool;
    std::vector<EventLoop*> m_loops;
};

}