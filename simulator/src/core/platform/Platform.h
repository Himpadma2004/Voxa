#pragma once

#include <string>

namespace VOXA
{
    /// Abstract platform interface.
    ///
    /// Provides hardware/OS capabilities in a testable, portable way.
    /// The simulator implements this with SimulatorPlatform.
    /// A real ESP32 build would provide its own concrete implementation.
    class Platform
    {
    public:
        virtual ~Platform() = default;

        // ---------------------------------------------------------------
        // Storage
        // ---------------------------------------------------------------

        /// Returns the root path where data files should be stored.
        /// e.g. "/data" on ESP32, "data/" in the simulator.
        [[nodiscard]] virtual std::string getStoragePath() const = 0;

        // ---------------------------------------------------------------
        // Power
        // ---------------------------------------------------------------

        /// Returns battery percentage 0–100.
        [[nodiscard]] virtual int getBatteryLevel() const = 0;

        /// Returns true if the device is currently charging.
        [[nodiscard]] virtual bool isCharging() const = 0;

        // ---------------------------------------------------------------
        // Time
        // ---------------------------------------------------------------

        /// Returns a human-readable date+time string.
        /// e.g. "Friday, Jun 26  11:44 AM"
        [[nodiscard]] virtual std::string getCurrentTimeString() const = 0;

        // ---------------------------------------------------------------
        // Device info
        // ---------------------------------------------------------------

        /// Returns a short platform identifier string, e.g. "simulator" or "esp32".
        [[nodiscard]] virtual std::string platformName() const = 0;
    };
}
