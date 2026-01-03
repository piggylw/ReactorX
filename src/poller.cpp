#include "reactor/poller.h"
#include "reactor/channel.h"
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>

namespace reactor
{

const int initialEventCount = 64;

const int kNew = -1;     // Channel未添加到epoll
const int kAdded = 1;    // Channel已添加到epoll
const int kDeleted = 2;  // Channel已从epoll删除

Poller::Poller()
    :m_epollfd(epoll_create1(EPOLL_CLOEXEC)),
     m_events(initialEventCount)
{
    if(m_epollfd < 0)
    {
        std::cerr << "Failed to create epoll file descriptor: " << strerror(errno) << std::endl;
        abort();
    }
}

Poller::~Poller()
{
    if(close(m_epollfd) < 0)
    {
        std::cerr << "Failed to close epoll file descriptor: " << strerror(errno) << std::endl;
    }
}

Poller::ChannelList Poller::poll(int timeoutMs)
{
    int numEvents = epoll_wait(m_epollfd, m_events.data(), static_cast<int>(m_events.size()), timeoutMs);

    ChannelList activeChannels;
    if(numEvents > 0)
    {
        std::cout << "Poller::poll() " << numEvents << " events happened" << std::endl;
        fillActiveChannels(numEvents, activeChannels);

        // 如果活跃的事件数量超过当前事件列表的大小，扩展事件列表
        if(static_cast<size_t>(numEvents) == m_events.size())
        {
            m_events.resize(m_events.size() * 2);
        }
    }
    else if(numEvents == 0)
    {
        // 超时，没有事件发生
        std::cout << "epoll_wait timeout" << std::endl;
    }
    else
    {
        if(errno != EINTR)
        {
            // 发生错误，输出错误信息
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
        }
    }

    return activeChannels;
}

void Poller::fillActiveChannels(int numEvents, ChannelList& activeChannels) const
{
    for(int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(m_events[i].data.ptr);
        assert(channel != nullptr);

        // 设置实际发生的事件
        channel->setRevents(m_events[i].events);
        activeChannels.push_back(channel);
    }
}

void Poller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    const int fd = channel->fd();
    std::cout << "Poller::updateChannel() fd=" << fd << " events=" << channel->events() << std::endl;

    // 获取该Channel在epoll中的状态

    if (index == kNew || index == kDeleted)
    {
        // 新Channel或已删除的Channel，需要添加到epoll
        if (index == kNew) 
        {
            assert(m_channels.find(fd) == m_channels.end());
            m_channels[fd] = channel;
        } 
        else 
        {
            // kDeleted
            assert(m_channels.find(fd) != m_channels.end());
            assert(m_channels[fd] == channel);
        }

        channel->setIndex(kAdded); // 设置为已添加状态

        struct epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.events = channel->events();
        event.data.ptr = channel;

        if (::epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &event) < 0) 
        {
            std::cerr << "Poller::updateChannel() epoll_ctl ADD failed: " 
                      << strerror(errno) << std::endl;
            abort();
        }
    } 
    else 
    {
        // kAdded，已在epoll中
        assert(index == kAdded);
        assert(m_channels.find(fd) != m_channels.end());

        if (channel->isNoneEvent()) 
        {
            // 没有关心任何事件，从epoll中删除
            if (::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) 
            {
                std::cerr << "Poller::updateChannel() epoll_ctl DEL failed: "
                          << strerror(errno) << std::endl;
                abort();
            }
            channel->setIndex(kDeleted);  // 标记为已删除
        } 
        else 
        {
            // 修改事件
            struct epoll_event event;
            std::memset(&event, 0, sizeof(event));
            event.events = channel->events();
            event.data.ptr = channel;

            if (::epoll_ctl(m_epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
                std::cerr << "Poller::updateChannel() epoll_ctl MOD failed: "
                          << strerror(errno) << std::endl;
                abort();
            }
        }
    }
}

void Poller::removeChannel(Channel* channel)
{
    const int fd = channel->fd();
    const int index = channel->index();
    std::cout << "Poller::removeChannel() fd = " << fd << std::endl;

    assert(m_channels.find(fd) != m_channels.end());
    assert(channel->isNoneEvent());
    assert(channel == m_channels[fd]);
    assert(index == kAdded || index == kDeleted);
    
    m_channels.erase(fd);

    // 只有在kAdded状态才需要从epoll删除
    // 如果是kDeleted，说明已经通过disableAll()删除过了
    if (index == kAdded) 
    {
        if (::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) 
        {
            std::cerr << "Poller::removeChannel() epoll_ctl DEL failed: "
                      << strerror(errno) << std::endl;
            abort();
        }
    }
    channel->setIndex(kNew); // 设置为新状态
}

}