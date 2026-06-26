#include "SimulatorPlatform.h"

#include <chrono>
#include <ctime>
#include <string>

namespace VOXA
{
    // -----------------------------------------------------------------------
    // Storage
    // -----------------------------------------------------------------------

    std::string SimulatorPlatform::getStoragePath() const
    {
        // Relative to the current working directory (next to the .exe in Debug/).
        return "data";
    }

    // -----------------------------------------------------------------------
    // Power  (fully mocked for the simulator)
    // -----------------------------------------------------------------------

    int SimulatorPlatform::getBatteryLevel() const
    {
        return 92; // Mock: always 92%
    }

    bool SimulatorPlatform::isCharging() const
    {
        return false; // Mock: not charging
    }

    // -----------------------------------------------------------------------
    // Time  (real system clock)
    // -----------------------------------------------------------------------

    std::string SimulatorPlatform::getCurrentTimeString() const
    {
        auto now       = std::chrono::system_clock::now();
        std::time_t t  = std::chrono::system_clock::to_time_t(now);
        std::tm local  {};

#if defined(_MSC_VER)
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif

        char buf[128];
        std::strftime(buf, sizeof(buf), "%A, %b %d  %I:%M %p", &local);
        return std::string(buf);
    }

    // -----------------------------------------------------------------------
    // Identity
    // -----------------------------------------------------------------------

    std::string SimulatorPlatform::platformName() const
    {
        return "simulator";
    }
}
