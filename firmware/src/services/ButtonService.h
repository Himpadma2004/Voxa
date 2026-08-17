#pragma once

#include <Arduino.h>
#include <cstdint>

namespace VOXA
{
    // Hardware configuration for the physical tactile button
    // GPIO 1 was freed by removing the SD Card adapter.
    // Connected to GND: Pressed = LOW, Released = HIGH (internal pull-up).
    constexpr gpio_num_t PHYSICAL_BUTTON_PIN = GPIO_NUM_1;

    class ButtonService
    {
    public:
        ButtonService();

        /// Initialize the physical button GPIO with internal pull-up and start background monitor task
        void begin(gpio_num_t pin = PHYSICAL_BUTTON_PIN);

        /// Background task loop
        void taskLoop();

        /// Internal tick called by taskLoop
        void tick();

        /// Check if the physical button is currently held down
        bool isPressed() const { return m_isPressed; }

        /// Check if a long-press triggered voice recording request is pending (auto-clears on read)
        bool hasPendingRecordTrigger();
        void clearPendingRecordTrigger();

        /// Check if a physical button stop request is pending (auto-clears on read)
        bool hasPendingStopTrigger();
        void clearPendingStopTrigger();

        /// Reset all button trigger states & set a lockout cooldown
        void resetAndCooldown(uint32_t cooldownDurationMs = 800);

        static bool isDirectRecordRequested();
        static void requestDirectRecord();
        static void clearDirectRecordRequest();

    private:
        gpio_num_t m_pin { PHYSICAL_BUTTON_PIN };
        volatile bool m_isPressed { false };
        volatile bool m_recordTriggerFired { false };
        volatile bool m_pendingRecordTrigger { false };
        volatile bool m_pendingStopTrigger { false };

        uint32_t m_pressStartTime { 0 };
        uint32_t m_lastDebounceTime { 0 };
        uint32_t m_cooldownUntil { 0 };
        int m_lastRawState { HIGH };

        static constexpr uint32_t DEBOUNCE_MS = 40;
        static constexpr uint32_t HOLD_TO_RECORD_MS = 300;
    };

    extern ButtonService buttonService;
}
