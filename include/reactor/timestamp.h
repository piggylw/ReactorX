#pragma once

#include <cstdint>
#include <string>

namespace reactor
{

class Timestamp
{
public:
    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : m_microSecondsSinceEpoch(microSecondsSinceEpoch) {}
    
    Timestamp()
        : m_microSecondsSinceEpoch(0) {}

    bool valid() const { return m_microSecondsSinceEpoch > 0; }
    int64_t microSecondsSinceEpoch() const { return m_microSecondsSinceEpoch; }
    double seconds() const { return static_cast<double>(m_microSecondsSinceEpoch) / kMicroSecondsPerSecond; }

    //格式化
    std::string toString() const;
    std::string toFormattedString(bool showMicroseconds = true) const;

    //静态方法
    static Timestamp now();
    static Timestamp invalid() { return Timestamp(); }

    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t m_microSecondsSinceEpoch; // 微秒时间戳
};

inline bool operator<(const Timestamp& lhs, const Timestamp& rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline double timeDifference(const Timestamp& high, const Timestamp& low)
{
    return (high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch()) / static_cast<double>(Timestamp::kMicroSecondsPerSecond);
}

inline Timestamp addTime(const Timestamp& timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}