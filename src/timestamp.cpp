#include "reactor/timestamp.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace reactor
{

std::string Timestamp::toString() const
{
    int64_t seconds = m_microSecondsSinceEpoch / kMicroSecondsPerSecond;
    int64_t microseconds = m_microSecondsSinceEpoch % kMicroSecondsPerSecond;
    std::ostringstream oss;
    oss << seconds << '.' << std::setfill('0') << std::setw(6) << microseconds;
    return oss.str();
}    

std::string Timestamp::toFormattedString(bool showMicroseconds) const
{
    time_t seconds = static_cast<time_t>(m_microSecondsSinceEpoch / kMicroSecondsPerSecond);
    struct tm time;
    gmtime_r(&seconds, &time);
    std::ostringstream oss;
    if(showMicroseconds)
    {
        oss << std::put_time(&time, "%Y-%m-%d %H:%M:%S.") 
            << std::setfill('0') << std::setw(6) 
            << (m_microSecondsSinceEpoch % kMicroSecondsPerSecond);
    }
    else
    {
        oss << std::put_time(&time, "%Y-%m-%d %H:%M:%S");
    }
    return oss.str();
}

Timestamp Timestamp::now()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t microSecondsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    return Timestamp(microSecondsSinceEpoch);
}


}