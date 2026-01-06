#include "reactor/eventloopthreadpool.h"
#include "reactor/eventloop.h"
#include "reactor/eventloopthread.h"
#include <cassert>

namespace reactor 
{
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop)
    : m_baseloop(baseloop),
      m_started(false),
      m_threadNums(0),
      m_next(0)
{}

EventLoopThreadPool::~EventLoopThreadPool()
{
    
}

void EventLoopThreadPool::start()
{
    assert(!m_started);
    assert(m_baseloop != nullptr);
    m_baseloop->assertInLoopThread();

    m_started = true;
    for(int i = 0; i < m_threadNums; ++i)
    {
        auto thread = std::make_unique<EventLoopThread>();
        m_loops.push_back(thread->startLoop());
        m_pool.push_back(std::move(thread));
    }
    // 如果m_threadNums == 0，loops为空
    // getNextLoop()会返回baseLoop
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    assert(m_started);
    assert(m_baseloop != nullptr);
    m_baseloop->assertInLoopThread();

    EventLoop* loop = m_baseloop;
    if(!m_loops.empty())
    {
        loop = m_loops[m_next];
        m_next = (m_next + 1) % m_loops.size();
    }
    
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    assert(m_started);
    assert(m_baseloop != nullptr);
    m_baseloop->assertInLoopThread();

    if(m_loops.empty())
        return std::vector<EventLoop*>(1, m_baseloop);
    else
        return m_loops;
}

}//reactor