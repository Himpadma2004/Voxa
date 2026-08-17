#pragma once

#include <Arduino.h>
#include <cstdint>

namespace VOXA
{
    /**
     * @brief Intelligent Power & Sleep Manager for VOXA hardware.
     *
     * Features:
     * - Smartphone-style sleep/wake with display & backlight power-down.
     * - Physical power button (GPIO 1) short press: sleep/wake toggle.
     * - Physical power button (GPIO 1) long press: push-to-talk recording / power-on hold.
     * - Clean phone-style Restart (stops tasks, flushes memory, soft reboots).
     * - Deep Sleep Shutdown with GPIO 1 button wake (hold to power on).
     * - Factory Reset (clears NVS, Wi-Fi, SPIFFS caches).
     */
    class PowerManager
    {
    public:
        PowerManager(const PowerManager&) = delete;
        PowerManager& operator=(const PowerManager&) = delete;

        static PowerManager& instance();

        /// Initialize power manager and activity timers
        void begin(uint32_t autoSleepTimeoutMs = 30000);

        /// Call every loop() tick or background task
        void tick();

        /// Register user activity (e.g. touch, UI interaction, button press)
        void reportActivity();

        /// Put device into low-power display sleep mode (screen off)
        void sleep();

        /// Wake up device and restore display backlight
        void wakeup();

        /// Toggle between sleep and awake state (e.g. on short power button click)
        void toggleSleep();

        /// Check if device is currently sleeping
        [[nodiscard]] bool isSleeping() const { return m_isSleeping; }

        /// Get configured auto-sleep timeout in milliseconds
        [[nodiscard]] uint32_t getTimeoutMs() const { return m_timeoutMs; }

        /// Set auto-sleep timeout (e.g. 15000, 30000, 60000, 0 = disabled)
        void setTimeoutMs(uint32_t timeoutMs);

        // ── Full System Power Actions ─────────────────────────────────────────

        /// Performs clean smartphone-style restart with visual feedback & task teardown
        void restartDevice();

        /// Performs deep-sleep shutdown with GPIO 1 wake (hold button to power on)
        void shutdownDevice();

        /// Performs factory reset (clears WiFi, NVS, and local temp files) and reboots
        void factoryReset();

    private:
        PowerManager() = default;

        void renderPowerScreen(const char* title, const char* subtitle, uint16_t accentColor);

        bool     m_isSleeping       { false };
        uint32_t m_timeoutMs        { 30000 }; // 30 seconds default
        uint32_t m_lastActivityMs   { 0 };
        uint8_t  m_savedBrightness  { 130 };
    };
}
