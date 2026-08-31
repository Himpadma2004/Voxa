#include "ButtonService.h"
#include "MicrophoneService.h"
#include "PowerManager.h"
#include "../audio/AudioManager.h"

namespace VOXA
{
    ButtonService buttonService;
    static volatile bool s_directRecordFlag = false;

    bool ButtonService::isDirectRecordRequested()
    {
        return s_directRecordFlag;
    }

    void ButtonService::requestDirectRecord()
    {
        s_directRecordFlag = true;
    }

    void ButtonService::clearDirectRecordRequest()
    {
        s_directRecordFlag = false;
    }

    ButtonService::ButtonService()
    {
    }

    void ButtonService::begin(gpio_num_t pin)
    {
        m_pin = pin;
        pinMode(m_pin, INPUT_PULLUP);
        m_lastRawState = digitalRead(m_pin);
        m_isPressed = (m_lastRawState == LOW);
        m_recordTriggerFired = false;
        m_pendingRecordTrigger = false;
        m_pendingStopTrigger = false;
        m_cooldownUntil = 0;

        Serial.printf("[ButtonService] Initialized on GPIO %d (Active LOW, Internal PULLUP)\n", (int)m_pin);

        // Start dedicated background polling task on Core 1
        xTaskCreatePinnedToCore(
            [](void* param) { static_cast<ButtonService*>(param)->taskLoop(); },
            "BtnTask",
            3072,
            this,
            2, // Priority 2
            nullptr,
            1  // Core 1
        );
    }

    void ButtonService::taskLoop()
    {
        Serial.println("[ButtonService] Background monitoring task running on Core 1");
        while (true)
        {
            tick();
            vTaskDelay(pdMS_TO_TICKS(15)); // 66 Hz polling rate
        }
    }

    void ButtonService::tick()
    {
        uint32_t now = millis();
        int raw = digitalRead(m_pin);

        // Software debouncing
        if (raw != m_lastRawState)
        {
            m_lastDebounceTime = now;
            m_lastRawState = raw;
        }

        if ((now - m_lastDebounceTime) < DEBOUNCE_MS)
        {
            return;
        }

        bool currentlyLow = (raw == LOW);

        // Ignore presses during cooldown lockout (protects against release-bounce)
        if (now < m_cooldownUntil)
        {
            m_isPressed = currentlyLow;
            return;
        }

        // State Transition: Just Pressed Down
        if (currentlyLow && !m_isPressed)
        {
            m_isPressed = true;
            m_pressStartTime = now;
            m_recordTriggerFired = false;
            m_pendingStopTrigger = false;
            Serial.printf("[ButtonService] Button Pressed down @ %u ms\n", now);
        }
        // State: Holding Button Down
        else if (currentlyLow && m_isPressed)
        {
            uint32_t holdDuration = now - m_pressStartTime;

            // Trigger recording after hold threshold (300ms)
            if (!m_recordTriggerFired && holdDuration >= HOLD_TO_RECORD_MS)
            {
                m_recordTriggerFired = true;

                // If sleeping, wake up first
                if (PowerManager::instance().isSleeping())
                {
                    PowerManager::instance().wakeup();
                }
                else
                {
                    PowerManager::instance().reportActivity();
                }

                m_pendingRecordTrigger = true;
                s_directRecordFlag = true;
                m_pendingStopTrigger = false;
                Serial.printf("[ButtonService] Hold detected (%u ms) -> Triggering Voice Recording Start\n", holdDuration);
            }
        }
        // State Transition: Just Released (Button let go)
        else if (!currentlyLow && m_isPressed)
        {
            m_isPressed = false;
            uint32_t totalDuration = now - m_pressStartTime;
            Serial.printf("[ButtonService] Button Released after %u ms\n", totalDuration);

            // If recording was triggered or is currently recording, releasing the button STOPS it immediately!
            if (m_recordTriggerFired || microphoneService.isRecording())
            {
                m_pendingStopTrigger = true;
                m_pendingRecordTrigger = false;
                s_directRecordFlag = false;
                // Set anti-bounce cooldown for 700ms so mechanical release vibration cannot start a new recording
                m_cooldownUntil = now + 700;
                Serial.println("[ButtonService] Stop & Upload Triggered on Button Release. Cooldown locked for 700ms.");
            }
            else if (totalDuration >= 50 && totalDuration < HOLD_TO_RECORD_MS)
            {
                // Short Press (50ms - 300ms): Clean Power Button Sleep / Wake Toggle
                PowerManager::instance().toggleSleep();
                m_cooldownUntil = now + 350; // 350ms anti-bounce cooldown
                Serial.printf("[ButtonService] Power Button Pressed (%u ms) -> Toggled Sleep/Wake\n", totalDuration);
            }

            m_recordTriggerFired = false;
        }
    }

    bool ButtonService::hasPendingRecordTrigger()
    {
        if (m_pendingRecordTrigger)
        {
            m_pendingRecordTrigger = false;
            return true;
        }
        return false;
    }

    void ButtonService::clearPendingRecordTrigger()
    {
        m_pendingRecordTrigger = false;
        s_directRecordFlag = false;
    }

    bool ButtonService::hasPendingStopTrigger()
    {
        if (m_pendingStopTrigger)
        {
            m_pendingStopTrigger = false;
            return true;
        }
        return false;
    }

    void ButtonService::clearPendingStopTrigger()
    {
        m_pendingStopTrigger = false;
    }

    void ButtonService::resetAndCooldown(uint32_t cooldownDurationMs)
    {
        m_pendingRecordTrigger = false;
        m_pendingStopTrigger = false;
        m_recordTriggerFired = false;
        s_directRecordFlag = false;
        m_cooldownUntil = millis() + cooldownDurationMs;
    }
}
