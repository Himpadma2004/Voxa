#pragma once

namespace VOXA
{
    class Platform;

    /// Provides battery status information.
    ///
    /// Delegates to the injected Platform abstraction so the service
    /// works identically on the simulator and on real hardware.
    class BatteryService
    {
    public:
        /// @param platform  Non-owning pointer. Must outlive this service.
        explicit BatteryService(Platform* platform);

        /// Returns current battery percentage (0–100).
        [[nodiscard]] int  getBatteryLevel() const;

        /// Returns true if the device is currently connected to power.
        [[nodiscard]] bool isCharging() const;

        /// Returns a short human-readable status string, e.g. "92% — Not charging".
        [[nodiscard]] const char* statusString() const;

    private:
        Platform* m_platform { nullptr };
    };
}
