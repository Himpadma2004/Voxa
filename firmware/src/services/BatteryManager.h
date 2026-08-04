#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace VOXA
{
    // ── Hardware Configuration ────────────────────────────────────────────────
    // Confirmed-free GPIO pins on this VOXA board:
    //   Taken: 1(SD-CS) 2(SD-MOSI) 3(BL) 4(I2S-BCLK) 5(I2S-LRCK) 6(Bat-ADC)
    //          7(I2S-DATA) 8(Touch-SDA) 9(DC) 10(Disp-CS) 11(Disp-MOSI) 12(Disp-SCLK)
    //          13(SD-MISO) 14(Disp-RST) 15(Mic-VCC) 16(Touch-INT) 17(Touch-RST) 18(Touch-SCL) 21(SD-SCK)
    static constexpr uint8_t  BAT_ADC_PIN     = 6;   // GPIO6 — ADC1_CH5 (free, battery voltage divider)
    static constexpr uint8_t  CHARGING_PIN    = 255; // 255 = Disabled/Unassigned (avoids stealing Touch I2C SDA pin 8)
    static constexpr uint32_t BAT_REFRESH_MS  = 5000; // Refresh voltage every 5 seconds
    static constexpr uint32_t BAT_ANIM_MS     = 1000; // Advance charging animation every second
    static constexpr int      BAT_ADC_SAMPLES = 8;    // Samples to average for noise rejection
    static constexpr int      BAT_ICON_FRAMES = 5;    // 0–4 representing 0%, 25%, 50%, 75%, 100%

    // ── BatteryManager ────────────────────────────────────────────────────────
    /// Singleton that reads real ADC voltage from a voltage-divided battery pin,
    /// converts it to a Li-Po percentage, tracks charging state, drives status-bar
    /// animation frames, and emits threshold warnings (20 / 10 / 5 %).
    class BatteryManager
    {
    public:
        BatteryManager(const BatteryManager&) = delete;
        BatteryManager& operator=(const BatteryManager&) = delete;

        /// Returns the single shared instance.
        static BatteryManager& instance();

        /// Call once in setup() to configure ADC / charging pin.
        void begin();

        /// Call every loop() tick — drives timers for refresh and animation.
        void tick();

        // ── Getters used by the UI ────────────────────────────────────────────

        /// Current battery percentage (0–100).
        [[nodiscard]] int  getPercentage()  const { return m_percentage; }

        /// True while USB/charger power is detected.
        [[nodiscard]] bool isCharging()     const { return m_charging; }

        /// True when charging and battery is at 100%.
        [[nodiscard]] bool isFull()         const { return m_charging && m_percentage >= 100; }

        /// Icon frame for drawing (0–4).
        /// While charging (and not full) cycles 0→1→2→3→4→0 every second.
        /// While not charging returns a frame proportional to the real percentage.
        /// When fully charged returns 4 (static full icon).
        [[nodiscard]] int  getIconFrame()   const { return m_iconFrame; }

    private:
        BatteryManager() = default;

        // ── Internal helpers ──────────────────────────────────────────────────
        void  updateStatus();
        float readVoltage();
        int   voltageToPercent(float voltage) const;
        bool  readChargingPin() const;
        void  checkWarningThresholds(int oldPct, int newPct);
        void  updateIconFrame();

        // ── State ─────────────────────────────────────────────────────────────
        int      m_percentage  { 85 };   // starts at a sane default
        bool     m_charging    { false };
        int      m_iconFrame   { 4 };    // 0-4

        uint32_t m_lastRefreshMs { 0 };
        uint32_t m_lastAnimMs    { 0 };
        int      m_animFrame     { 0 };  // cycles 0..4 during charging animation

        // Simulation state (used when ADC reading is out of plausible range)
        float    m_simPct        { 85.0f };
        bool     m_simMode       { false };

        // Warning edge-detection: which thresholds have already fired this session
        bool     m_warned20      { false };
        bool     m_warned10      { false };
        bool     m_warned5       { false };
    };

} // namespace VOXA
