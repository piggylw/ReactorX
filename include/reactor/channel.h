#pragma once

#include "noncopyable.h"
#include "callbacks.h"


namespace reactor
{

class EventLoop;

class Channel : private NonCopyable
{
public:
    explicit Channel(EventLoop* loop, int fd);
    ~Channel();

    //回调函数
    void setReadCallback(EventCallback cb) { m_readCallback = std::move(cb); }
    void setWriteCallback(EventCallback cb) { m_writeCallback = std::move(cb); }
    void setCloseCallback(EventCallback cb) { m_closeCallback = std::move(cb); }
    void setErrorCallback(EventCallback cb) { m_errorCallback = std::move(cb); }

    int fd() const { return m_fd; }

    uint32_t events() const { return m_events; }
    void setRevents(uint32_t revents) { m_revents = revents; }

    //是否没有关心任何事件
    bool isNoneEvent() const { return m_events == kNoneEvent; }

    //启用事件
    void enableReading() { m_events |= kReadEvent; update(); }
    void enableWriting() { m_events |= kWriteEvent; update(); }
    void disableWriting() { m_events &= ~kWriteEvent; update(); }
    void disableAll() { m_events = kNoneEvent; update(); }
    void disableReading() { m_events &= ~kReadEvent; update(); }

    //判断当前状态
    bool isReading() const { return m_events & kReadEvent; }
    bool isWriting() const { return m_events & kWriteEvent; }

    // 事件处理（由Eventloop调用）
    void handleEvent();

    // 从Poller中移除
    void remove();

    EventLoop* ownerLoop() const { return m_loop; }

    // Poller状态索引
    int index() const { return m_index; }
    void setIndex(int idx) { m_index = idx; }
    
private:
    void update();

    //事件常量(epoll的事件类型)
    static const uint32_t kNoneEvent;
    static const uint32_t kReadEvent;
    static const uint32_t kWriteEvent;

    EventLoop* m_loop; // EventLoop对象指针
    const int m_fd; // 文件描述符
    uint32_t m_events; // 关心事件
    uint32_t m_revents; // 实际发生的事件
    int m_index;  // 在Poller中的状态（kNew=-1, kAdded=1, kDeleted=2）

    //回调
    EventCallback m_readCallback; // 读事件回调
    EventCallback m_writeCallback; // 写事件回调
    EventCallback m_closeCallback; // 关闭事件回调
    EventCallback m_errorCallback; // 错误事件回调

    bool m_eventHandling; // 是否正在处理事件

};

}