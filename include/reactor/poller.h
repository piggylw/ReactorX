#pragma once

#include "noncopyable.h"
#include <sys/epoll.h>
#include <vector>
#include <unordered_map>

namespace reactor
{

class Channel;

class Poller : private NonCopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller();
    ~Poller();

    // 等待事件发生
    // timeout: 超时时间（毫秒），-1表示永久阻塞
    // 返回：活跃的Channel列表
    ChannelList poll(int timeoutMs = -1);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    
private:
    void fillActiveChannels(int numEvents, ChannelList& activeChannels) const;

    int m_epollfd; // epoll文件描述符

    using EventList = std::vector<struct epoll_event>;
    EventList m_events; // epoll事件列表

    // 管理所有的Channel
    // key: 文件描述符, value: Channel*
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap m_channels; // 管理Channel的映射表
};

}