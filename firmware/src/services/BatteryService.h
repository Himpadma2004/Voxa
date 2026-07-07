#pragma once

namespace VOXA
{
    /// Provides battery status information.
    class BatteryService
    {
    public:
        explicit BatteryService(void* platform = nullptr);

        /// Returns current battery percentage (0–100).
        [[nodiscard]] int  getBatteryLevel() const;

        /// Returns true if the device is currently connected to power.
        [[nodiscard]] bool isCharging() const;

        /// Returns a short human-readable status string, e.g. "92% — Not charging".
        [[nodiscard]] const char* statusString() const;
    };
}
