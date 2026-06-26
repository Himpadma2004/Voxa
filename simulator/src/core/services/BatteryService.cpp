#include "BatteryService.h"
#include "../platform/Platform.h"

#include <cstdio>
#include <string>

namespace VOXA
{
    BatteryService::BatteryService(Platform* platform)
        : m_platform(platform)
    {
    }

    int BatteryService::getBatteryLevel() const
    {
        if (m_platform == nullptr) return 0;
        return m_platform->getBatteryLevel();
    }

    bool BatteryService::isCharging() const
    {
        if (m_platform == nullptr) return false;
        return m_platform->isCharging();
    }

    const char* BatteryService::statusString() const
    {
        // Static buffer — acceptable for a status display string.
        static char buf[64];
        const int level = getBatteryLevel();
        const bool charging = isCharging();
        std::snprintf(buf, sizeof(buf),
                      "%d%%%s", level, charging ? " — Charging" : " — Not charging");
        return buf;
    }
}
