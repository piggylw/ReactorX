#include "reactor/eventloop.h"
#include "reactor/channel.h"
#include "reactor/eventloopthreadpool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <atomic>

using namespace reactor;

// 全局统计
std::atomic<int> g_totalConnections{0};
std::atomic<int64_t> g_totalBytesRead{0};
std::atomic<int64_t> g_totalBytesWritten{0};

struct Connection {
    int fd;
    EventLoop* loop;
    std::unique_ptr<Channel> channel;
    
    Connection(EventLoop* l, int sockfd) 
        : fd(sockfd), loop(l), channel(new Channel(l, sockfd)) {}
};

// 每个线程的连接管理器（无需加锁）
thread_local std::map<int, std::shared_ptr<Connection>> t_connections;

int createListenSocket(uint16_t port)
{
    int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd < 0) abort();

    int optval = 1;
    ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) abort();
    if (::listen(listenfd, 128) < 0) abort();

    std::cout << "Server listening on 0.0.0.0:" << port << std::endl;
    return listenfd;
}

void onNewConnection(EventLoopThreadPool* pool, int listenfd)
{
    struct sockaddr_in peerAddr;
    socklen_t addrLen = sizeof(peerAddr);
    
    int connfd = ::accept4(listenfd, (struct sockaddr*)&peerAddr, &addrLen,
                           SOCK_NONBLOCK | SOCK_CLOEXEC);
    
    if (connfd < 0) return;

    char buf[32];
    ::inet_ntop(AF_INET, &peerAddr.sin_addr, buf, sizeof(buf));
    std::cout << "New connection from " << buf << ":" << ntohs(peerAddr.sin_port) 
              << " fd=" << connfd << std::endl;

    // 获取下一个EventLoop（负载均衡）
    EventLoop* ioLoop = pool->getNextLoop();
    
    // 在选定的Loop线程执行连接初始化
    ioLoop->runInLoop([ioLoop, connfd]() {
        // 此时已在ioLoop线程
        auto conn = std::make_shared<Connection>(ioLoop, connfd);
        
        conn->channel->setReadCallback([conn]() {
            char buf[4096];
            ssize_t n = ::read(conn->fd, buf, sizeof(buf));
            
            if (n > 0) {
                g_totalBytesRead.fetch_add(n);
                
                // Echo back
                ssize_t nw = ::write(conn->fd, buf, n);
                if (nw > 0) {
                    g_totalBytesWritten.fetch_add(nw);
                }
            } else if (n == 0) {
                std::cout << "[Thread " << tid() << "] "
                          << "Connection closed, fd=" << conn->fd << std::endl;
                
                conn->channel->disableAll();
                conn->channel->remove();
                ::close(conn->fd);
                t_connections.erase(conn->fd);
                
                g_totalConnections.fetch_sub(1);
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "read error: " << strerror(errno) << std::endl;
                    
                    conn->channel->disableAll();
                    conn->channel->remove();
                    ::close(conn->fd);
                    t_connections.erase(conn->fd);
                    
                    g_totalConnections.fetch_sub(1);
                }
            }
        });
        
        conn->channel->enableReading();
        t_connections[connfd] = conn;
        
        g_totalConnections.fetch_add(1);
    });
}

void printStatistics()
{
    std::cout << "\n=== Statistics ===" << std::endl;
    std::cout << "Connections: " << g_totalConnections.load() << std::endl;
    std::cout << "Total read: " << g_totalBytesRead.load() << " bytes" << std::endl;
    std::cout << "Total written: " << g_totalBytesWritten.load() << " bytes" << std::endl;
    std::cout << "==================\n" << std::endl;
}

int main(int argc, char* argv[])
{
    int numThreads = 4;
    if (argc > 1) {
        numThreads = atoi(argv[1]);
    }

    std::cout << "Multi-threaded Echo Server ===" << std::endl;
    std::cout << "Thread pool size: " << numThreads << std::endl;

    EventLoop loop;  // 主Loop（Acceptor）
    
    // 创建线程池
    EventLoopThreadPool pool(&loop);
    pool.setThreadNum(numThreads);
    pool.start();

    int listenfd = createListenSocket(9983);

    Channel listenChannel(&loop, listenfd);
    listenChannel.setReadCallback([&pool, listenfd]() {
        onNewConnection(&pool, listenfd);
    });
    listenChannel.enableReading();

    // 定期打印统计信息
    loop.runEvery(5.0, printStatistics);

    std::cout << "\nServer started. Use:\n"
              << "  Single client: nc localhost 9983\n"
              << std::endl;

    loop.loop();

    ::close(listenfd);
    return 0;
}