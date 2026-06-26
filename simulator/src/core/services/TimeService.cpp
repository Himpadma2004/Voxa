#include "TimeService.h"

#include <chrono>
#include <ctime>
#include <string>

namespace
{
    /// Fill a std::tm with the current local time.
    std::tm localNow()
    {
        auto now      = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local {};
#if defined(_MSC_VER)
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        return local;
    }
}

namespace VOXA
{
    std::string TimeService::getCurrentTime() const
    {
        std::tm local = localNow();
        char buf[32];
        std::strftime(buf, sizeof(buf), "%I:%M %p", &local);
        return std::string(buf);
    }

    std::string TimeService::getCurrentDate() const
    {
        std::tm local = localNow();
        char buf[64];
        std::strftime(buf, sizeof(buf), "%A, %b %d", &local);
        return std::string(buf);
    }

    std::string TimeService::getFormattedDateTime() const
    {
        std::tm local = localNow();
        char buf[128];
        std::strftime(buf, sizeof(buf), "%A, %b %d  %I:%M %p", &local);
        return std::string(buf);
    }

    int TimeService::getCurrentYear() const
    {
        std::tm local = localNow();
        return local.tm_year + 1900;
    }
}
