#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace reactor
{

inline pid_t tid()
{
    thread_local pid_t cacheedTid = 0;
    if (cacheedTid == 0)
    {
        cacheedTid = static_cast<pid_t>(syscall(SYS_gettid));
    }
    return cacheedTid;
}

inline bool isMainThread()
{
    return tid() == getpid();
}

}