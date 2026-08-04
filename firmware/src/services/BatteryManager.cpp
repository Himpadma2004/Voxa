#include "BatteryManager.h"
#include <Arduino.h>

namespace VOXA
{
    // ── Singleton ─────────────────────────────────────────────────────────────
    BatteryManager& BatteryManager::instance()
    {
        static BatteryManager s_instance;
        return s_instance;
    }

    // ── begin() ───────────────────────────────────────────────────────────────
    void BatteryManager::begin()
    {
        // Configure ADC pin for battery voltage (input only, no pull resistors)
        pinMode(BAT_ADC_PIN, INPUT);

        // Charging detect pin: active LOW (TP4056 /CHRG pin or similar)
        if (CHARGING_PIN != 255)
        {
            pinMode(CHARGING_PIN, INPUT_PULLUP);
        }

        // Set 11 dB attenuation ONLY on our battery pin (0–3.3 V full-scale).
        // Using pin-specific call so we do NOT disturb any other ADC channel
        // or LEDC / PWM peripheral (e.g. the display backlight on GPIO3).
        analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);

        // Perform an initial read so the status bar shows a real value from frame 1
        updateStatus();

        Serial.printf("[BatteryManager] Initialized — %d%% | %s | ADC=GPIO%d CHG=%s\n",
                      m_percentage,
                      m_charging ? "Charging" : "Not charging",
                      BAT_ADC_PIN, CHARGING_PIN == 255 ? "Disabled" : String(CHARGING_PIN).c_str());
    }

    // ── tick() ────────────────────────────────────────────────────────────────
    void BatteryManager::tick()
    {
        uint32_t now = millis();

        // ── Refresh hardware reading every BAT_REFRESH_MS (5 s) ──────────────
        if (now - m_lastRefreshMs >= BAT_REFRESH_MS)
        {
            m_lastRefreshMs = now;
            updateStatus();
        }

        // ── Advance charging animation frame every BAT_ANIM_MS (1 s) ─────────
        if (m_charging && !isFull())
        {
            if (now - m_lastAnimMs >= BAT_ANIM_MS)
            {
                m_lastAnimMs = now;
                m_animFrame = (m_animFrame + 1) % BAT_ICON_FRAMES;
            }
        }

        updateIconFrame();
    }

    // ── updateStatus() ────────────────────────────────────────────────────────
    void BatteryManager::updateStatus()
    {
        int oldPct = m_percentage;

        // 1. Read charging pin (active LOW on most Li-Po charger ICs)
        m_charging = readChargingPin();

        // 2. Read and convert battery voltage
        float voltage = readVoltage();

        // 3. Plausibility check: a real Li-Po in a system always reads 2.5V–4.4V.
        //    If the ADC is floating / unconnected, fall back to simulation.
        if (voltage < 2.0f || voltage > 5.0f)
        {
            m_simMode = true;

            // In simulation mode, slowly charge or discharge depending on state
            if (m_charging)
            {
                m_simPct += 0.4f;
                if (m_simPct > 100.0f) m_simPct = 100.0f;
            }
            else
            {
                m_simPct -= 0.15f;
                if (m_simPct < 1.0f) m_simPct = 1.0f;
            }
            m_percentage = static_cast<int>(m_simPct);
        }
        else
        {
            m_simMode    = false;
            m_percentage = voltageToPercent(voltage);
        }

        // Clamp
        m_percentage = constrain(m_percentage, 0, 100);

        // 4. Warn on low-battery threshold crossings
        checkWarningThresholds(oldPct, m_percentage);

        Serial.printf("[BatteryManager] %.3fV → %d%% | %s%s\n",
                      voltage,
                      m_percentage,
                      m_charging ? "Charging" : "Not charging",
                      m_simMode  ? " [sim]"   : "");
    }

    // ── readVoltage() ─────────────────────────────────────────────────────────
    // Averages BAT_ADC_SAMPLES readings for noise rejection.
    // The voltage divider (100 kΩ / 100 kΩ) halves VBAT before it reaches the ADC,
    // so we multiply the measured value by 2 to recover the true battery voltage.
    float BatteryManager::readVoltage()
    {
        uint32_t sum = 0;
        for (int i = 0; i < BAT_ADC_SAMPLES; ++i)
        {
            sum += analogRead(BAT_ADC_PIN);
            delay(1);
        }
        float avg = static_cast<float>(sum) / BAT_ADC_SAMPLES;

        // ESP32-S3 ADC: 12-bit (0–4095) at 3.3 V full-scale (11 dB attenuation)
        float measuredV = (avg / 4095.0f) * 3.3f;

        // Multiply by divider ratio to get actual battery voltage
        float batteryV = measuredV * 2.0f;

        return batteryV;
    }

    // ── voltageToPercent() ────────────────────────────────────────────────────
    // Piecewise-linear approximation of a standard Li-Po 3.7 V discharge curve.
    // Verified against empirical discharge data for 300–3000 mAh single-cell packs.
    int BatteryManager::voltageToPercent(float voltage) const
    {
        struct Point { float v; int pct; };

        // Voltage breakpoints (descending) — measured open-circuit or light load
        static const Point curve[] =
        {
            { 4.20f, 100 },
            { 4.15f,  95 },
            { 4.10f,  90 },
            { 4.00f,  80 },
            { 3.90f,  65 },
            { 3.85f,  50 },
            { 3.80f,  35 },
            { 3.75f,  25 },
            { 3.70f,  15 },
            { 3.60f,   9 },
            { 3.50f,   5 },
            { 3.40f,   2 },
            { 3.30f,   0 },
        };

        constexpr int N = sizeof(curve) / sizeof(curve[0]);

        if (voltage >= curve[0].v)  return 100;
        if (voltage <= curve[N-1].v) return 0;

        // Find the surrounding segment and linearly interpolate
        for (int i = 0; i < N - 1; ++i)
        {
            if (voltage >= curve[i + 1].v)
            {
                float v0 = curve[i].v,     v1 = curve[i + 1].v;
                int   p0 = curve[i].pct,   p1 = curve[i + 1].pct;
                return p1 + static_cast<int>(
                    (voltage - v1) / (v0 - v1) * static_cast<float>(p0 - p1));
            }
        }
        return 0;
    }

    // ── readChargingPin() ─────────────────────────────────────────────────────
    bool BatteryManager::readChargingPin() const
    {
        if (CHARGING_PIN == 255) return false;

        // Active LOW: the TP4056 /CHRG pin is pulled LOW while charging,
        // and floating/HIGH when not charging (INPUT_PULLUP handles the HIGH side).
        return (digitalRead(CHARGING_PIN) == LOW);
    }

    // ── checkWarningThresholds() ──────────────────────────────────────────────
    // Fires once per session when the percentage crosses a threshold downward.
    void BatteryManager::checkWarningThresholds(int oldPct, int newPct)
    {
        if (!m_warned20 && oldPct > 20 && newPct <= 20)
        {
            m_warned20 = true;
            Serial.println("[BatteryManager] ⚠ WARNING: Battery low — 20%!");
        }
        if (!m_warned10 && oldPct > 10 && newPct <= 10)
        {
            m_warned10 = true;
            Serial.println("[BatteryManager] ⚠ WARNING: Battery critical — 10%!");
        }
        if (!m_warned5 && oldPct > 5 && newPct <= 5)
        {
            m_warned5 = true;
            Serial.println("[BatteryManager] ⚠ WARNING: Battery emergency — 5%! Please charge now.");
        }

        // Reset edge flags if battery recovers above the threshold (e.g. started charging)
        if (newPct > 20) { m_warned20 = false; }
        if (newPct > 10) { m_warned10 = false; }
        if (newPct >  5) { m_warned5  = false; }
    }

    // ── updateIconFrame() ─────────────────────────────────────────────────────
    void BatteryManager::updateIconFrame()
    {
        if (isFull())
        {
            // Fully charged → static full frame, no animation
            m_iconFrame = BAT_ICON_FRAMES - 1; // 4
        }
        else if (m_charging)
        {
            // Animate: frame cycles 0→1→2→3→4→0 (driven by tick())
            m_iconFrame = m_animFrame;
        }
        else
        {
            // Not charging: frame reflects the real battery level
            // Map 0-100% → 0-4 frames
            if      (m_percentage >= 87) m_iconFrame = 4;
            else if (m_percentage >= 62) m_iconFrame = 3;
            else if (m_percentage >= 37) m_iconFrame = 2;
            else if (m_percentage >= 12) m_iconFrame = 1;
            else                         m_iconFrame = 0;
        }
    }

} // namespace VOXA
