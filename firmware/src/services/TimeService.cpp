#include "TimeService.h"
#include <Arduino.h>

#include <chrono>
#include <ctime>
#include <string>

#include <sys/time.h>

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
    void TimeService::begin()
    {
        configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
        Serial.println("[TimeService] NTP Configured for Asia/Kolkata (GMT+5:30)");
    }

    std::string TimeService::getCurrentTime() const
    {
        std::tm local = localNow();
        char buf[32];
        std::strftime(buf, sizeof(buf), "%I:%M %p", &local);
        std::string s(buf);
        if (!s.empty() && s[0] == '0') s = s.substr(1);
        return s;
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

    std::string TimeService::getISO8601Time() const
    {
        std::tm local = localNow();
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &local);
        return std::string(buf);
    }

    int TimeService::getCurrentYear() const
    {
        std::tm local = localNow();
        return local.tm_year + 1900;
    }

    void TimeService::setTime(int hour, int minute, int second, int day, int month, int year)
    {
        struct tm t;
        t.tm_sec = second;
        t.tm_min = minute;
        t.tm_hour = hour;
        t.tm_mday = day;
        t.tm_mon = month - 1; // 0-based month
        t.tm_year = year - 1900;
        t.tm_isdst = -1;
        time_t timeSinceEpoch = mktime(&t);
        struct timeval tv = { .tv_sec = timeSinceEpoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }
}

