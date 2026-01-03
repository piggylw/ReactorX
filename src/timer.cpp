#include "reactor/timer.h"

namespace reactor
{
std::atomic<int64_t> Timer::s_sequence(0);

void Timer::restart(Timestamp now)
{
    if (m_repeat)
    {
        // 计算下一个到期时间
        m_expiration = addTime(now, m_interval);
    }
    else
    {
        // 单次定时器，设置为无效时间
        m_expiration = Timestamp::invalid();
    }
}


}