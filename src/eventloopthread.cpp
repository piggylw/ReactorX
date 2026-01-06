#include "reactor/eventloopthread.h"
#include "reactor/eventloop.h"

namespace reactor 
{

EventLoopThread::EventLoopThread()
    : m_loop(nullptr)
{}

EventLoopThread::~EventLoopThread()
{
    if(m_loop != nullptr)
    {
        m_loop->quit();
        if(m_thread.joinable())
            m_thread.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    m_thread = std::thread(&EventLoopThread::threadFunc, this);
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(lock, [this](){ return m_loop != nullptr; });
    }
    return m_loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_loop = &loop;
    }
    m_cond.notify_all();

    loop.loop();

    std::unique_lock<std::mutex> lock(m_mtx);
    m_loop = nullptr;
}

}