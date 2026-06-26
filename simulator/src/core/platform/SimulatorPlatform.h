#pragma once

#include "Platform.h"

namespace VOXA
{
    /// Desktop / simulator implementation of Platform.
    ///
    /// - Battery is mocked (fixed values).
    /// - Time comes from the system clock via std::chrono / std::ctime.
    /// - Storage path is a relative "data/" folder next to the executable.
    class SimulatorPlatform final : public Platform
    {
    public:
        SimulatorPlatform() = default;

        // ---------------------------------------------------------------
        // Storage
        // ---------------------------------------------------------------
        [[nodiscard]] std::string getStoragePath() const override;

        // ---------------------------------------------------------------
        // Power (mocked)
        // ---------------------------------------------------------------
        [[nodiscard]] int  getBatteryLevel() const override;
        [[nodiscard]] bool isCharging()      const override;

        // ---------------------------------------------------------------
        // Time (real system clock)
        // ---------------------------------------------------------------
        [[nodiscard]] std::string getCurrentTimeString() const override;

        // ---------------------------------------------------------------
        // Identity
        // ---------------------------------------------------------------
        [[nodiscard]] std::string platformName() const override;
    };
}
