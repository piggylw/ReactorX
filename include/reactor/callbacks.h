#pragma once
#include <functional>
#include <memory>

namespace reactor
{

using EventCallback = std::function<void()>;
using TimerCallback = std::function<void()>;
using Functor = std::function<void()>;

}// namespace reactor