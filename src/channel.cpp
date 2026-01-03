#include "reactor/channel.h"
#include "reactor/eventloop.h"
#include <sys/epoll.h>
#include <cassert>
#include <iostream>

namespace reactor 
{
// epoll 事件常量
const uint32_t Channel::kNoneEvent = 0;
const uint32_t Channel::kReadEvent = EPOLLIN | EPOLLPRI;  // 可读 + 紧急数据
const uint32_t Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : m_loop(loop), m_fd(fd), m_events(0), m_revents(0), m_index(-1), m_eventHandling(false)
{
    assert(loop != nullptr);
    assert(fd >= 0);
}

Channel::~Channel()
{
    assert(!m_eventHandling); // 确保在销毁前没有事件正在处理
}

void Channel::update()
{
    m_loop->updateChannel(this);
}

void Channel::remove()
{
    assert(isNoneEvent()); // 确保在移除前没有关心的事件
    m_loop->removeChannel(this);
}

void Channel::handleEvent()
{
    m_eventHandling = true;

    // EPOLLHUP: 对端关闭连接（挂起）
    // EPOLLERR: 错误
    // EPOLLRDHUP: 对端关闭连接或半关闭写端（Linux 2.6.17+）

    if((m_revents & EPOLLHUP) && !(m_revents & EPOLLIN))
    {
        //发生挂起但没有读事件，调用关闭回调
        if(m_closeCallback)
        {
            std::cout << "Channel::handleEvent() EPOLLHUP, calling close callback" << std::endl;
            m_closeCallback();
        }
    }

    if(m_revents & EPOLLERR)
    {
        //发生错误，调用错误回调
        if(m_errorCallback)
        {
            std::cout << "Channel::handleEvent() EPOLLERR, calling error callback" << std::endl;
            m_errorCallback();
        }
    }

    if(m_revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        //发生可读或紧急数据事件，调用读回调
        if(m_readCallback)
        {
            std::cout << "Channel::handleEvent() EPOLLIN or EPOLLPRI, calling read callback" << std::endl;
            m_readCallback();
        }
    }

    if(m_revents & EPOLLOUT)
    {
        //发生可写事件，调用写回调
        if(m_writeCallback)
        {
            std::cout << "Channel::handleEvent() EPOLLOUT, calling write callback" << std::endl;
            m_writeCallback();
        }
    }

    m_eventHandling = false; // 事件处理完成

}

}