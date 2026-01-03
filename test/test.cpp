#include "reactor/eventloop.h"
#include "reactor/channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <map>

using namespace reactor;

// 创建监听socket
int createListenSocket(uint16_t port)
{
    int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd < 0) {
        std::cerr << "socket() failed" << std::endl;
        abort();
    }

    int optval = 1;
    ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind() failed" << std::endl;
        abort();
    }

    if (::listen(listenfd, 128) < 0) {
        std::cerr << "listen() failed" << std::endl;
        abort();
    }

    std::cout << "Server listening on 0.0.0.0:" << port << std::endl;
    return listenfd;
}

// 连接管理结构（简化版，不是完整的TcpConnection）
struct Connection {
    int fd;
    std::unique_ptr<Channel> channel;
    
    Connection(EventLoop* loop, int sockfd) 
        : fd(sockfd), channel(new Channel(loop, sockfd)) 
    {
    }
};

// 全局连接管理器（使用智能指针）
std::map<int, std::shared_ptr<Connection>> g_connections;

// 接受新连接的回调
void onNewConnection(EventLoop* loop, int listenfd)
{
    struct sockaddr_in peerAddr;
    socklen_t addrLen = sizeof(peerAddr);
    
    int connfd = ::accept4(listenfd, (struct sockaddr*)&peerAddr, &addrLen,
                           SOCK_NONBLOCK | SOCK_CLOEXEC);
    
    if (connfd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "accept4() failed: " << strerror(errno) << std::endl;
        }
        return;
    }

    char buf[32];
    ::inet_ntop(AF_INET, &peerAddr.sin_addr, buf, sizeof(buf));
    std::cout << "New connection from " << buf << ":" << ntohs(peerAddr.sin_port) 
              << " fd=" << connfd << std::endl;

    // 使用shared_ptr管理连接
    auto conn = std::make_shared<Connection>(loop, connfd);

    // 设置读回调：echo逻辑
    conn->channel->setReadCallback([conn]() {
        char buf[1024];
        ssize_t n = ::read(conn->fd, buf, sizeof(buf));
        
        if (n > 0) {
            std::cout << "Read " << n << " bytes from fd=" << conn->fd << std::endl;
            
            // Echo back
            ssize_t nw = ::write(conn->fd, buf, n);
            if (nw < 0) {
                std::cerr << "write() error: " << strerror(errno) << std::endl;
            }
        } else if (n == 0) {
            // 对端关闭连接
            std::cout << "Connection closed by peer, fd=" << conn->fd << std::endl;
            
            // 关闭连接
            conn->channel->disableAll();
            conn->channel->remove();
            ::close(conn->fd);
            
            // 从管理器中移除（shared_ptr自动释放内存）
            g_connections.erase(conn->fd);
        } else {
            // 出错
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "read() error: " << strerror(errno) << std::endl;
                
                conn->channel->disableAll();
                conn->channel->remove();
                ::close(conn->fd);
                g_connections.erase(conn->fd);
            }
        }
    });

    // 设置关闭回调
    conn->channel->setCloseCallback([conn]() {
        std::cout << "Connection closed, fd=" << conn->fd << std::endl;
        conn->channel->disableAll();
        conn->channel->remove();
        ::close(conn->fd);
        g_connections.erase(conn->fd);
    });

    // 设置错误回调
    conn->channel->setErrorCallback([conn]() {
        std::cerr << "Connection error, fd=" << conn->fd << std::endl;
        conn->channel->disableAll();
        conn->channel->remove();
        ::close(conn->fd);
        g_connections.erase(conn->fd);
    });

    // 启用读事件
    conn->channel->enableReading();
    
    // 加入连接管理器
    g_connections[connfd] = conn;
}

int main()
{
    std::cout << "=== Reactor Day2 Test: Echo Server (Multiple Connections) ===" << std::endl;

    EventLoop loop;

    int listenfd = createListenSocket(9981);

    Channel listenChannel(&loop, listenfd);
    
    listenChannel.setReadCallback([&loop, listenfd]() {
        onNewConnection(&loop, listenfd);
    });

    listenChannel.enableReading();

    std::cout << "EventLoop starting..." << std::endl;
    std::cout << "Use: telnet localhost 9981 or nc localhost 9981" << std::endl;
    
    loop.loop();

    ::close(listenfd);
    
    return 0;
}