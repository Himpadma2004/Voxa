#include "BatteryService.h"
#include <cstdio>
#include <string>

namespace VOXA
{
    BatteryService::BatteryService(void* platform)
    {
    }

    int BatteryService::getBatteryLevel() const
    {
        return 88;
    }

    bool BatteryService::isCharging() const
    {
        return false;
    }

    const char* BatteryService::statusString() const
    {
        static char buf[64];
        const int level = getBatteryLevel();
        const bool charging = isCharging();
        std::snprintf(buf, sizeof(buf),
                      "%d%%%s", level, charging ? " — Charging" : " — Not charging");
        return buf;
    }
}
