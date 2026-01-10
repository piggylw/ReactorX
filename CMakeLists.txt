cmake_minimum_required(VERSION 3.10)
project(ReactorCore)

# C++11标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")

# 头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件
set(REACTOR_SRCS
    src/poller.cpp
    src/channel.cpp
    src/eventloop.cpp
    src/timestamp.cpp      
    src/timer.cpp          
    src/timerqueue.cpp  
    src/eventloopthread.cpp
    src/eventloopthreadpool.cpp
)

# 生成静态库
add_library(reactor STATIC ${REACTOR_SRCS})

# 测试程序
add_subdirectory(test)