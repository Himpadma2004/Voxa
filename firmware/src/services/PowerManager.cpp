#include "PowerManager.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "MicrophoneService.h"
#include "WiFiManager.h"
#include "../audio/AudioManager.h"
#include "../storage/SpiffsMutex.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

namespace VOXA
{
    PowerManager& PowerManager::instance()
    {
        static PowerManager s_instance;
        return s_instance;
    }

    void PowerManager::begin(uint32_t autoSleepTimeoutMs)
    {
        m_timeoutMs = autoSleepTimeoutMs;
        m_lastActivityMs = millis();
        m_isSleeping = false;
        m_savedBrightness = Display::getBrightness();
        if (m_savedBrightness == 0) m_savedBrightness = 130;

        Serial.printf("[PowerManager] Initialized (Auto-Sleep Timeout: %u seconds)\n", m_timeoutMs / 1000);
    }

    void PowerManager::reportActivity()
    {
        m_lastActivityMs = millis();

        // If user touches screen or interacts while sleeping, wake up automatically
        if (m_isSleeping)
        {
            wakeup();
        }
    }

    void PowerManager::sleep()
    {
        if (m_isSleeping) return;

        // Never sleep while recording or uploading voice notes
        if (microphoneService.isBusy())
        {
            Serial.println("[PowerManager] Sleep aborted: MicrophoneService is busy.");
            m_lastActivityMs = millis();
            return;
        }

        uint8_t currBr = Display::getBrightness();
        if (currBr > 0)
        {
            m_savedBrightness = currBr;
        }

        Serial.println("[PowerManager] Entering Display Sleep Mode (Backlight OFF)...");

        // Turn off display backlight immediately (cuts power consumption instantly)
        Display::setBrightness(0);

        m_isSleeping = true;
        Serial.println("[PowerManager] Device is now Sleeping");
    }

    void PowerManager::wakeup()
    {
        if (!m_isSleeping) return;

        Serial.println("[PowerManager] Waking up device (Backlight ON)...");

        // Restore saved backlight brightness
        uint8_t targetBr = (m_savedBrightness > 0) ? m_savedBrightness : 130;
        Display::setBrightness(targetBr);

        m_isSleeping = false;
        m_lastActivityMs = millis();

        Serial.printf("[PowerManager] Device Awake (Backlight restored to %u/255)\n", targetBr);
    }

    void PowerManager::toggleSleep()
    {
        if (m_isSleeping)
        {
            wakeup();
        }
        else
        {
            sleep();
        }
    }

    void PowerManager::setTimeoutMs(uint32_t timeoutMs)
    {
        m_timeoutMs = timeoutMs;
        m_lastActivityMs = millis();
        Serial.printf("[PowerManager] Auto-Sleep Timeout set to %u ms\n", timeoutMs);
    }

    void PowerManager::tick()
    {
        if (m_isSleeping)
        {
            return;
        }

        // Check if recording is active -> continuously reset activity timer
        if (microphoneService.isBusy())
        {
            m_lastActivityMs = millis();
            return;
        }

        // Check inactivity timeout
        if (m_timeoutMs > 0 && (millis() - m_lastActivityMs >= m_timeoutMs))
        {
            Serial.printf("[PowerManager] Inactivity timeout (%u seconds) reached -> Auto-Sleep Triggered\n",
                          m_timeoutMs / 1000);
            sleep();
        }
    }

    void PowerManager::renderPowerScreen(const char* title, const char* subtitle, uint16_t accentColor)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        Display::setBrightness(150);
        Display::lcd.fillScreen(VoxaTheme::getBackground());

        // Center card container
        float cx = w * 0.5f;
        float cy = h * 0.45f;

        Display::lcd.fillCircle((int)cx, (int)cy - 10, 24, accentColor);
        Display::lcd.fillCircle((int)cx, (int)cy - 10, 18, VoxaTheme::getBackground());

        // Title
        Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
        Display::lcd.setTextColor(VoxaTheme::getTextPrimary(), VoxaTheme::getBackground());
        Display::lcd.setTextDatum(textdatum_t::middle_center);
        Display::lcd.drawString(title, cx, cy + 30);

        // Subtitle
        Display::lcd.setFont(&fonts::FreeSans9pt7b);
        Display::lcd.setTextColor(VoxaTheme::getTextSecondary(), VoxaTheme::getBackground());
        Display::lcd.drawString(subtitle, cx, cy + 55);

        // Visual progress bar
        uint16_t barW = w * 0.6f;
        uint16_t barH = 4;
        uint16_t barX = cx - (barW * 0.5f);
        uint16_t barY = cy + 85;

        Display::lcd.fillRoundRect(barX, barY, barW, barH, 2, VoxaTheme::getDivider());
        Display::lcd.fillRoundRect(barX, barY, barW * 0.7f, barH, 2, accentColor);
    }

    void PowerManager::restartDevice()
    {
        Serial.println("[PowerManager] Initiating clean system restart...");

        renderPowerScreen("Restarting...", "Closing active tasks & clearing RAM", 0xFD20);
        AudioManager::instance().playTone(1000, 100);

        // Gracefully terminate active audio & network services
        AudioManager::instance().stop();
        if (microphoneService.isRecording())
        {
            microphoneService.stopRecording("PowerManager::restart", "reboot");
        }
        WiFi.disconnect(true);

        delay(800);
        Serial.println("[PowerManager] Executing esp_restart()...");
        esp_restart();
    }

    void PowerManager::shutdownDevice()
    {
        Serial.println("[PowerManager] Initiating deep-sleep power off...");

        renderPowerScreen("Powering Off", "Hold button to power on", 0xF800);
        AudioManager::instance().playTone(600, 150);

        // Gracefully terminate services
        AudioManager::instance().stop();
        if (microphoneService.isRecording())
        {
            microphoneService.stopRecording("PowerManager::shutdown", "shutdown");
        }
        WiFi.disconnect(true);

        delay(1000);

        // Turn off display backlight completely
        Display::setBrightness(0);
        Display::lcd.fillScreen(TFT_BLACK);

        Serial.println("[PowerManager] Configuring Wakeup on GPIO 1 (Physical Power Button)...");
        pinMode(GPIO_NUM_1, INPUT_PULLUP);
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_1, 0); // Wakeup when GPIO 1 is pulled LOW

        Serial.println("[PowerManager] Entering Deep Sleep. Device is powered OFF.");
        esp_deep_sleep_start();
    }

    void PowerManager::factoryReset()
    {
        Serial.println("[PowerManager] Initiating Factory Reset...");

        renderPowerScreen("Factory Reset", "Restoring default settings...", 0xD000);
        AudioManager::instance().playTone(750, 200);

        // Clear Wi-Fi credentials
        wifiManager.clearCredentials();
        wifiManager.clearForcePortal();

        // Clear Non-Volatile Preferences
        Preferences preferences;
        preferences.begin("voxa", false);
        preferences.clear();
        preferences.end();

        // Clear temporary audio files from SPIFFS
        {
            SpiffsLock lock("PowerManager::factoryReset");
            File root = SPIFFS.open("/recordings");
            if (root && root.isDirectory())
            {
                File f = root.openNextFile();
                while (f)
                {
                    if (!f.isDirectory())
                    {
                        String name = f.name();
                        f.close();
                        SPIFFS.remove(name);
                    }
                    else f.close();
                    f = root.openNextFile();
                }
                root.close();
            }
        }

        delay(1200);
        Serial.println("[PowerManager] Factory reset complete. Rebooting...");
        esp_restart();
    }
}
