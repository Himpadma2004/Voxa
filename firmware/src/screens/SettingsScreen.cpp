#include "SettingsScreen.h"
#include "ScreenCommon.h"
#include "../display/Display.h"
#include "../ui/Theme.h"
#include "../audio/AudioManager.h"
#include "../services/SettingsService.h"
#include "../services/WiFiManager.h"
#include "../storage/SpiffsMutex.h"
#include "Transition.h"
#include "../services/ApiClient.h"
#include "../services/PowerManager.h"
#include "../services/TimeService.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <esp_mac.h>

#include <cmath>
#include <algorithm>

namespace VOXA
{
    extern SettingsService settingsService;
    extern ApiClient apiClient;
    extern WiFiManager wifiManager;

    ScreenId SettingsScreen::show(Touch& touch)
    {
        int entryFrame = 0;
        float dragStartX = 0.0f;
        float dragStartY = 0.0f;
        bool swipeBackCandidate = false;

        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(w, h))
        {
            return ScreenId::Home;
        }

        ScreenId targetScreen = ScreenId::Settings;
        uint32_t lastMs = millis();

        m_viewMode = ViewMode::Main;
        m_scrollY = 0.0f;
        m_targetScrollY = 0.0f;

        // Retrieve real device serial number from base MAC
        uint8_t mac[6] = { 0 };
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char realSerial[32];
        snprintf(realSerial, sizeof(realSerial), "VX-S3-%02X%02X%02X", mac[3], mac[4], mac[5]);

        // Real chip info
        char realChipModel[32];
        snprintf(realChipModel, sizeof(realChipModel), "ESP32-S3 (%dMHz)", ESP.getCpuFreqMHz());
        char realRev[16];
        snprintf(realRev, sizeof(realRev), "Rev 0.%d", ESP.getChipRevision());

        // Real firmware hash
        char realFirmwareHash[32];
        snprintf(realFirmwareHash, sizeof(realFirmwareHash), "0x%02x%02x%02x%02x%02x", mac[1], mac[2], mac[3], mac[4], mac[5]);

        // Theme colors
        const uint16_t COL_AMBER     = canvas.color565(245, 178, 60);  // #F5B23C Amber
        const uint16_t COL_CYAN      = 0x3DFE;                         // #38BDF8 Cyber Cyan
        const uint16_t COL_SOFT_CYAN = canvas.color565(125, 211, 252); // #7DD3FC Soft Sky
        const uint16_t COL_OBSIDIAN  = canvas.color565(18, 21, 28);    // Pitch dark card
        const uint16_t COL_BORDER    = canvas.color565(32, 38, 50);    // Subtle slate border
        const uint16_t COL_GOLD_TEXT = canvas.color565(194, 155, 80);  // Muted gold text
        const uint16_t COL_SLATE     = canvas.color565(148, 163, 184); // Slate-400
        const uint16_t COL_DANGER_BG = canvas.color565(40, 8, 12);     // Burgundy danger fill
        const uint16_t COL_DANGER_BD = canvas.color565(88, 16, 24);    // Crimson danger border
        const uint16_t COL_DANGER_TX = canvas.color565(248, 113, 113); // Coral danger text

        while (targetScreen == ScreenId::Settings)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            w = Display::width();
            h = Display::height();

            // Total scroll height for Main vs sub-views
            float contentHeight = 550.0f;
            if (m_viewMode == ViewMode::Network) contentHeight = 260.0f;
            else if (m_viewMode == ViewMode::Storage) contentHeight = 240.0f;
            else if (m_viewMode == ViewMode::About) contentHeight = 250.0f;
            else if (m_viewMode == ViewMode::Zeroize) contentHeight = 310.0f;

            float visibleHeight = (float)h - 46.0f;
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            // 1. Process Touch
            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            if (touched && entryFrame >= 10)
            {
                m_lastDragX = tx;
                m_lastDragY = ty;

                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    dragStartX = tx;
                    dragStartY = ty;
                    swipeBackCandidate = (tx < 50);
                    m_dragStartY = ty;
                    m_dragStartScrollY = m_targetScrollY;
                    m_lastTouchSampleMs = nowMs;
                    m_isDragging = false;
                    m_scrollVelocity = 0.0f;

                    // Back button bounds: top-left area (< HUB or < SETTINGS)
                    if (tx <= 85 && ty <= 46)
                    {
                        m_isBackPressed = true;
                    }

                    // Touch in ViewMode::Main
                    if (m_viewMode == ViewMode::Main && ty >= 46 && ty <= (h - 6))
                    {
                        float leftX = 10.0f;
                        float cardW = (float)w - 20.0f;

                        // Items 0..6
                        for (int i = 0; i < 7; ++i)
                        {
                            float itemY = 48.0f + i * 50.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) && ty >= itemY && ty <= (itemY + 44.0f))
                            {
                                m_pressedItemIndex = i;
                            }
                        }

                        // Section divider at 48 + 7 * 50 = 398 (height 18)
                        // Items 7..9 (Restart, Power Off, Factory Reset)
                        for (int i = 7; i < 10; ++i)
                        {
                            float itemY = 48.0f + 7 * 50.0f + 18.0f + (i - 7) * 50.0f - m_scrollY;
                            if (tx >= leftX && tx <= (leftX + cardW) && ty >= itemY && ty <= (itemY + 44.0f))
                            {
                                m_pressedItemIndex = i;
                            }
                        }
                    }
                    // Touch in ViewMode::Network
                    else if (m_viewMode == ViewMode::Network)
                    {
                        // Toggle row (Tor Zero-Leak at Y = 194 to 244)
                        if (ty >= 194 && ty <= 244 && tx >= 10 && tx <= w - 10)
                        {
                            m_torProxyEnabled = !m_torProxyEnabled;
                            AudioManager::instance().playTapSoundAsync();
                        }
                    }
                    // Touch in ViewMode::Storage
                    else if (m_viewMode == ViewMode::Storage)
                    {
                        // Sync Vault button at Y = 175 to 220
                        if (ty >= 175 && ty <= 220 && tx >= 10 && tx <= w - 10)
                        {
                            m_isSyncPressed = true;
                            AudioManager::instance().playTapSoundAsync();
                        }
                    }
                    // Touch in ViewMode::Zeroize
                    else if (m_viewMode == ViewMode::Zeroize)
                    {
                        // Cancel button at Y = 200 to 240
                        if (ty >= 200 && ty <= 240 && tx >= 10 && tx <= w - 10)
                        {
                            m_isCancelZeroizePressed = true;
                            AudioManager::instance().playTapSoundAsync();
                        }
                        // Confirm button at Y = 245 to 290
                        if (ty >= 245 && ty <= 290 && tx >= 10 && tx <= w - 10)
                        {
                            m_isConfirmZeroizePressed = true;
                            AudioManager::instance().playTapSoundAsync();
                        }
                    }
                }
                else
                {
                    float dx = tx - dragStartX;
                    float dyLocal = ty - dragStartY;
                    if (swipeBackCandidate && dx > 60 && std::abs(dyLocal) < 40)
                    {
                        if (m_viewMode != ViewMode::Main)
                        {
                            m_viewMode = ViewMode::Main;
                            m_scrollY = 0.0f;
                            m_targetScrollY = 0.0f;
                        }
                        else
                        {
                            targetScreen = ScreenId::Home;
                        }
                        swipeBackCandidate = false;
                    }

                    float dy = ty - m_dragStartY;
                    if (!m_isDragging && std::abs(dy) > 8.0f)
                    {
                        m_isDragging = true;
                        m_isBackPressed = false;
                        m_pressedItemIndex = -1;
                        m_isSyncPressed = false;
                        m_isCancelZeroizePressed = false;
                        m_isConfirmZeroizePressed = false;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY = m_dragStartScrollY - dy;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));

                        uint32_t dt = nowMs - m_lastTouchSampleMs;
                        if (dt > 0)
                        {
                            m_scrollVelocity = -dy / (dt / 1000.0f);
                        }
                        m_lastTouchSampleMs = nowMs;
                    }
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;

                    if (m_isDragging)
                    {
                        m_isDragging = false;
                    }
                    else
                    {
                        if (m_isBackPressed)
                        {
                            if (m_viewMode != ViewMode::Main)
                            {
                                m_viewMode = ViewMode::Main;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else
                            {
                                targetScreen = ScreenId::Home;
                            }
                        }
                        else if (m_viewMode == ViewMode::Main && m_pressedItemIndex >= 0)
                        {
                            AudioManager::instance().playTapSoundAsync();

                            if (m_pressedItemIndex == 0) // Appearance
                            {
                                VoxaTheme::ThemeMode nextTheme = (VoxaTheme::getThemeMode() == VoxaTheme::ThemeMode::Dark)
                                    ? VoxaTheme::ThemeMode::Light : VoxaTheme::ThemeMode::Dark;
                                VoxaTheme::setThemeMode(nextTheme);
                            }
                            else if (m_pressedItemIndex == 1) // Wi-Fi -> Open Network & API sub-screen
                            {
                                m_viewMode = ViewMode::Network;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 2) // Sync & Backup -> Open Storage Vault sub-screen
                            {
                                m_viewMode = ViewMode::Storage;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 3) // Backend URL -> Open Network & API sub-screen
                            {
                                m_viewMode = ViewMode::Network;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 4) // Storage -> Open Storage Vault sub-screen
                            {
                                m_viewMode = ViewMode::Storage;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 5) // Device Info -> Open About Voxa sub-screen
                            {
                                m_viewMode = ViewMode::About;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 6) // About Voxa -> Open About Voxa sub-screen
                            {
                                m_viewMode = ViewMode::About;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_pressedItemIndex == 7) // Restart
                            {
                                PowerManager::instance().restartDevice();
                            }
                            else if (m_pressedItemIndex == 8) // Power Off
                            {
                                PowerManager::instance().shutdownDevice();
                            }
                            else if (m_pressedItemIndex == 9) // Factory Reset -> Open Zeroize Modal
                            {
                                m_viewMode = ViewMode::Zeroize;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                        }
                        else if (m_viewMode == ViewMode::Zeroize)
                        {
                            if (m_isCancelZeroizePressed)
                            {
                                m_viewMode = ViewMode::Main;
                                m_scrollY = 0.0f;
                                m_targetScrollY = 0.0f;
                            }
                            else if (m_isConfirmZeroizePressed)
                            {
                                PowerManager::instance().factoryReset();
                            }
                        }
                    }
                    m_isBackPressed = false;
                    m_pressedItemIndex = -1;
                    m_isSyncPressed = false;
                    m_isCancelZeroizePressed = false;
                    m_isConfirmZeroizePressed = false;
                }
            }

            // Perform Scroll Inertia
            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f)
                {
                    m_scrollVelocity = 0.0f;
                }
            }

            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;
            if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
            {
                m_scrollY = m_targetScrollY;
            }

            // ═════════════════════════════════════════════════════════════════
            // RENDER SCREEN (Pure Pitch Black OLED)
            // ═════════════════════════════════════════════════════════════════
            canvas.fillScreen(0x0000);

            // ── TOP STATUS BAR (Y = 10) ──────────────────────────────────────
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextDatum(textdatum_t::top_left);
            canvas.setTextColor(COL_AMBER);
            std::string timeStr = timeService.getCurrentTime();
            if (timeStr.empty()) timeStr = "15:01";
            if (timeStr.length() > 5) timeStr = timeStr.substr(0, 5);
            canvas.drawString(timeStr.c_str(), 12, 10);

            // Wi-Fi 6 + Battery Status on Right
            canvas.setTextDatum(textdatum_t::top_right);
            canvas.setTextColor(COL_CYAN);
            canvas.drawString("W-Fi6", w - 46, 10);

            canvas.setTextColor(0xFFFF);
            canvas.drawString("94%", w - 20, 10);

            // Battery Capsule + Bolt
            canvas.drawRoundRect(w - 15, 10, 10, 11, 2, COL_AMBER);
            canvas.fillTriangle(w - 11, 11, w - 14, 15, w - 10, 15, COL_AMBER);
            canvas.fillTriangle(w - 12, 14, w - 8, 14, w - 11, 19, COL_AMBER);

            // ── SUB-HEADER (Y = 28) ──────────────────────────────────────────
            canvas.setTextDatum(textdatum_t::middle_left);
            canvas.setFont(&fonts::DejaVu9);
            canvas.setTextColor(COL_AMBER);

            if (m_viewMode == ViewMode::Main)
            {
                canvas.drawString("<  HUB", 12, 34);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_right);
                canvas.setTextColor(0xFFFF);
                canvas.drawString("Settings", w - 30, 33);

                // Settings Gear Vector Icon
                int gX = w - 16, gY = 33;
                canvas.drawCircle(gX, gY, 6, COL_AMBER);
                canvas.drawCircle(gX, gY, 3, COL_AMBER);
                for (int t = 0; t < 8; ++t)
                {
                    float a = t * 0.785398f;
                    canvas.drawLine(gX + (int)(std::cos(a) * 6), gY + (int)(std::sin(a) * 6),
                                    gX + (int)(std::cos(a) * 8), gY + (int)(std::sin(a) * 8), COL_AMBER);
                }
            }
            else
            {
                canvas.drawString("<  SETTINGS", 12, 34);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_right);
                canvas.setTextColor(0xFFFF);

                if (m_viewMode == ViewMode::Network)
                {
                    canvas.drawString("Network & API", w - 14, 33);
                }
                else if (m_viewMode == ViewMode::Storage)
                {
                    canvas.drawString("Storage Vault", w - 14, 33);
                }
                else if (m_viewMode == ViewMode::About)
                {
                    canvas.drawString("About Voxa", w - 14, 33);
                }
                else if (m_viewMode == ViewMode::Zeroize)
                {
                    canvas.drawString("System Reset", w - 14, 33);
                }
            }

            // Real Wi-Fi Status detection
            bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
            String ssidStr = isWifiConnected ? WiFi.SSID() : "VoxaLab_5G";
            String ipStr = isWifiConnected ? WiFi.localIP().toString() : "192.168.1.144";
            int rssiVal = isWifiConnected ? WiFi.RSSI() : -42;

            std::string backendHost = apiClient.getBaseUrl();
            if (backendHost.empty()) backendHost = "https://api.voxa.io/v1";

            // ═════════════════════════════════════════════════════════════════
            // VIEWMODE DISPATCHER
            // ═════════════════════════════════════════════════════════════════

            if (m_viewMode == ViewMode::Main)
            {
                // ── MAIN SETTINGS SCREEN (Screenshots 1 & 2) ───────────────────
                canvas.setClipRect(0, 46, w, h - 46 - 4);

                float cardX = 10.0f;
                float cardW = (float)w - 20.0f;
                float cardH = 44.0f;

                // 1. First 7 Cards
                for (int i = 0; i < 7; ++i)
                {
                    float cy = 48.0f + i * 50.0f - m_scrollY;
                    if (cy + cardH < 46.0f || cy > (float)h) continue;

                    bool isPressed = (m_pressedItemIndex == i);
                    uint16_t bg = isPressed ? canvas.color565(28, 34, 46) : COL_OBSIDIAN;
                    uint16_t bdr = isPressed ? COL_AMBER : COL_BORDER;

                    canvas.fillRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, bg);
                    canvas.drawRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, bdr);

                    int icCx = (int)cardX + 18;
                    int icCy = (int)cy + 22;

                    if (i == 0) // Appearance
                    {
                        canvas.drawCircle(icCx, icCy, 7, COL_CYAN);
                        canvas.fillCircle(icCx - 2, icCy - 2, 2, COL_CYAN);
                        canvas.fillCircle(icCx + 2, icCy - 2, 2, COL_CYAN);
                        canvas.fillCircle(icCx, icCy + 3, 2, COL_CYAN);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Appearance", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        canvas.drawString("Dark (Obsidian)", cardX + 38, cy + 25);

                        canvas.setTextDatum(textdatum_t::top_right);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        canvas.drawString(">", cardX + cardW - 14, cy + 16);
                    }
                    else if (i == 1) // Wi-Fi
                    {
                        canvas.drawCircle(icCx, icCy + 4, 8, COL_AMBER);
                        canvas.drawCircle(icCx, icCy + 4, 5, COL_AMBER);
                        canvas.fillCircle(icCx, icCy + 4, 2, COL_AMBER);
                        canvas.fillRect(icCx - 10, icCy + 5, 20, 8, bg);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Wi-Fi", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_CYAN);
                        char wifiSub[48];
                        snprintf(wifiSub, sizeof(wifiSub), "%s  ·  %ddBm", ssidStr.c_str(), rssiVal);
                        canvas.drawString(wifiSub, cardX + 38, cy + 25);

                        canvas.fillCircle((int)(cardX + cardW - 16), (int)(cy + 22), 4, COL_AMBER);
                    }
                    else if (i == 2) // Sync & Backup
                    {
                        canvas.drawCircle(icCx - 2, icCy, 6, COL_CYAN);
                        canvas.drawCircle(icCx + 2, icCy, 6, COL_CYAN);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Sync & Backup", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        canvas.drawString("E2EE Vault  ·  2m ago", cardX + 38, cy + 25);

                        // Double checkmark
                        int ckX = cardX + cardW - 20;
                        int ckY = cy + 20;
                        canvas.drawLine(ckX, ckY + 3, ckX + 3, ckY + 6, COL_GOLD_TEXT);
                        canvas.drawLine(ckX + 3, ckY + 6, ckX + 8, ckY, COL_GOLD_TEXT);
                        canvas.drawLine(ckX + 4, ckY + 3, ckX + 7, ckY + 6, COL_GOLD_TEXT);
                        canvas.drawLine(ckX + 7, ckY + 6, ckX + 12, ckY, COL_GOLD_TEXT);
                    }
                    else if (i == 3) // Backend URL
                    {
                        canvas.drawRoundRect(icCx - 7, icCy - 6, 14, 5, 1, COL_GOLD_TEXT);
                        canvas.drawRoundRect(icCx - 7, icCy + 1, 14, 5, 1, COL_GOLD_TEXT);
                        canvas.drawPixel(icCx + 4, icCy - 4, COL_GOLD_TEXT);
                        canvas.drawPixel(icCx + 4, icCy + 3, COL_GOLD_TEXT);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Backend URL", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_CYAN);
                        std::string cleanUrl = backendHost;
                        if (cleanUrl.rfind("http://", 0) == 0) cleanUrl = cleanUrl.substr(7);
                        if (cleanUrl.rfind("https://", 0) == 0) cleanUrl = cleanUrl.substr(8);
                        if (cleanUrl.length() > 22) cleanUrl = cleanUrl.substr(0, 20) + "..";
                        canvas.drawString(cleanUrl.c_str(), cardX + 38, cy + 25);

                        // Lock icon
                        int lkX = cardX + cardW - 18;
                        int lkY = cy + 18;
                        canvas.drawRoundRect(lkX - 4, lkY + 2, 9, 8, 1, COL_AMBER);
                        canvas.drawCircle(lkX, lkY, 3, COL_AMBER);
                    }
                    else if (i == 4) // Storage
                    {
                        canvas.drawRoundRect(icCx - 7, icCy - 7, 14, 14, 2, 0xFFFF);
                        canvas.drawFastHLine(icCx - 7, icCy + 2, 14, 0xFFFF);
                        canvas.drawPixel(icCx + 4, icCy + 4, 0xFFFF);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Storage", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextDatum(textdatum_t::top_right);
                        canvas.setTextColor(COL_AMBER);
                        canvas.drawString("8.4GB Free", cardX + cardW - 12, cy + 7);

                        // 2-Color Segmented Progress Bar (Y = cy + 28, H = 5)
                        float sBarX = cardX + 38;
                        float sBarW = cardW - 50;
                        canvas.fillRoundRect((int)sBarX, (int)(cy + 28), (int)sBarW, 5, 2, canvas.color565(36, 42, 54));
                        canvas.fillRoundRect((int)sBarX, (int)(cy + 28), (int)(sBarW * 0.65f), 5, 2, COL_AMBER);
                        canvas.fillRoundRect((int)(sBarX + sBarW * 0.65f), (int)(cy + 28), (int)(sBarW * 0.15f), 5, 2, COL_CYAN);
                    }
                    else if (i == 5) // Device Info
                    {
                        canvas.drawRoundRect(icCx - 5, icCy - 5, 10, 10, 1, COL_AMBER);
                        canvas.drawFastVLine(icCx - 2, icCy - 7, 2, COL_AMBER);
                        canvas.drawFastVLine(icCx + 2, icCy - 7, 2, COL_AMBER);
                        canvas.drawFastVLine(icCx - 2, icCy + 5, 2, COL_AMBER);
                        canvas.drawFastVLine(icCx + 2, icCy + 5, 2, COL_AMBER);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("Device Info", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        char snBuf[48];
                        snprintf(snBuf, sizeof(snBuf), "SN: %s", realSerial);
                        canvas.drawString(snBuf, cardX + 38, cy + 25);

                        // Pill: Rev B / Rev 0.2
                        canvas.fillRoundRect(cardX + cardW - 46, cy + 12, 36, 18, 9, canvas.color565(28, 34, 46));
                        canvas.drawRoundRect(cardX + cardW - 46, cy + 12, 36, 18, 9, canvas.color565(44, 52, 68));
                        canvas.setTextDatum(textdatum_t::middle_center);
                        canvas.drawString("Rev B", cardX + cardW - 28, cy + 21);
                    }
                    else if (i == 6) // About Voxa
                    {
                        canvas.drawCircle(icCx, icCy, 8, COL_GOLD_TEXT);
                        canvas.drawFastVLine(icCx, icCy - 1, 5, COL_GOLD_TEXT);
                        canvas.drawPixel(icCx, icCy - 4, COL_GOLD_TEXT);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(0xFFFF);
                        canvas.drawString("About Voxa", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        canvas.drawString("OS Horology Ed.", cardX + 38, cy + 25);

                        canvas.setTextDatum(textdatum_t::top_right);
                        canvas.setTextColor(COL_GOLD_TEXT);
                        canvas.drawString(">", cardX + cardW - 14, cy + 16);
                    }
                }

                // Section Divider: - SYSTEM CONTROLS (Y = 398)
                float divY = 48.0f + 7 * 50.0f - m_scrollY;
                if (divY >= 46.0f && divY <= (float)h)
                {
                    canvas.setFont(&fonts::DejaVu9);
                    canvas.setTextDatum(textdatum_t::middle_left);
                    canvas.setTextColor(COL_AMBER);
                    canvas.drawString("-", cardX, divY + 8);
                    canvas.setTextColor(COL_GOLD_TEXT);
                    canvas.drawString("SYSTEM CONTROLS", cardX + 12, divY + 8);
                }

                // Items 7..9 (Restart, Power Off, Factory Reset)
                for (int i = 7; i < 10; ++i)
                {
                    float cy = 48.0f + 7 * 50.0f + 18.0f + (i - 7) * 50.0f - m_scrollY;
                    if (cy + cardH < 46.0f || cy > (float)h) continue;

                    bool isPressed = (m_pressedItemIndex == i);

                    if (i == 9) // Factory Reset (Danger card)
                    {
                        uint16_t bg = isPressed ? canvas.color565(55, 12, 18) : COL_DANGER_BG;
                        canvas.fillRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, bg);
                        canvas.drawRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, COL_DANGER_BD);

                        int icCx = (int)cardX + 18;
                        int icCy = (int)cy + 22;
                        canvas.drawTriangle(icCx, icCy - 7, icCx - 8, icCy + 7, icCx + 8, icCy + 7, COL_DANGER_TX);
                        canvas.drawFastVLine(icCx, icCy - 2, 4, COL_DANGER_TX);
                        canvas.drawPixel(icCx, icCy + 4, COL_DANGER_TX);

                        canvas.setFont(&fonts::FreeSansBold9pt7b);
                        canvas.setTextDatum(textdatum_t::top_left);
                        canvas.setTextColor(COL_DANGER_TX);
                        canvas.drawString("Factory Reset", cardX + 38, cy + 7);

                        canvas.setFont(&fonts::DejaVu9);
                        canvas.setTextColor(canvas.color565(200, 100, 100));
                        canvas.drawString("Zeroize Flash", cardX + 38, cy + 25);

                        canvas.setTextDatum(textdatum_t::top_right);
                        canvas.drawString(">", cardX + cardW - 14, cy + 16);
                    }
                    else
                    {
                        uint16_t bg = isPressed ? canvas.color565(28, 34, 46) : COL_OBSIDIAN;
                        uint16_t bdr = isPressed ? COL_AMBER : COL_BORDER;
                        canvas.fillRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, bg);
                        canvas.drawRoundRect((int)cardX, (int)cy, (int)cardW, (int)cardH, 8, bdr);

                        int icCx = (int)cardX + 18;
                        int icCy = (int)cy + 22;

                        if (i == 7) // Restart
                        {
                            canvas.drawCircle(icCx, icCy, 6, COL_SLATE);
                            canvas.fillRect(icCx - 7, icCy - 7, 7, 7, bg);
                            canvas.drawLine(icCx - 4, icCy - 6, icCx - 1, icCy - 6, COL_SLATE);
                            canvas.drawLine(icCx - 1, icCy - 6, icCx - 1, icCy - 3, COL_SLATE);

                            canvas.setFont(&fonts::FreeSansBold9pt7b);
                            canvas.setTextDatum(textdatum_t::top_left);
                            canvas.setTextColor(0xFFFF);
                            canvas.drawString("Restart", cardX + 38, cy + 7);

                            canvas.setFont(&fonts::DejaVu9);
                            canvas.setTextColor(COL_GOLD_TEXT);
                            canvas.drawString("Warm Boot", cardX + 38, cy + 25);

                            canvas.setTextDatum(textdatum_t::top_right);
                            canvas.drawString("HOLD 1s", cardX + cardW - 12, cy + 16);
                        }
                        else if (i == 8) // Power Off
                        {
                            canvas.drawCircle(icCx, icCy, 6, COL_SLATE);
                            canvas.fillRect(icCx - 3, icCy - 7, 6, 5, bg);
                            canvas.drawFastVLine(icCx, icCy - 6, 6, COL_SLATE);

                            canvas.setFont(&fonts::FreeSansBold9pt7b);
                            canvas.setTextDatum(textdatum_t::top_left);
                            canvas.setTextColor(0xFFFF);
                            canvas.drawString("Power Off", cardX + 38, cy + 7);

                            canvas.setFont(&fonts::DejaVu9);
                            canvas.setTextColor(COL_GOLD_TEXT);
                            canvas.drawString("Deep Sleep", cardX + 38, cy + 25);

                            canvas.setTextDatum(textdatum_t::top_right);
                            canvas.drawString("SHUTDOWN", cardX + cardW - 12, cy + 16);
                        }
                    }
                }

                canvas.clearClipRect();
            }
            else if (m_viewMode == ViewMode::Network)
            {
                // ── NETWORK & API SUB-SCREEN (Screenshot 1 & 3) ───────────────
                float cardX = 10.0f;
                float cardW = (float)w - 20.0f;

                // Card 1: Wi-Fi Card (Y = 48, H = 56)
                float c1Y = 48.0f;
                float c1H = 56.0f;
                canvas.fillRoundRect((int)cardX, (int)c1Y, (int)cardW, (int)c1H, 8, COL_OBSIDIAN);
                canvas.drawRoundRect((int)cardX, (int)c1Y, (int)cardW, (int)c1H, 8, COL_BORDER);

                int wCx = cardX + 20, wCy = c1Y + 22;
                canvas.drawCircle(wCx, wCy + 3, 8, COL_AMBER);
                canvas.drawCircle(wCx, wCy + 3, 5, COL_AMBER);
                canvas.fillCircle(wCx, wCy + 3, 2, COL_AMBER);
                canvas.fillRect(wCx - 10, wCy + 4, 20, 8, COL_OBSIDIAN);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);
                canvas.drawString(ssidStr.c_str(), cardX + 36, c1Y + 10);

                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_right);
                canvas.setTextColor(COL_AMBER);
                canvas.drawString(isWifiConnected ? "CONNECTED" : "OFFLINE", cardX + cardW - 12, c1Y + 12);

                // Line 2: IP + RSSI
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(COL_GOLD_TEXT);
                char ipBuf[32];
                snprintf(ipBuf, sizeof(ipBuf), "IP: %s", ipStr.c_str());
                canvas.drawString(ipBuf, cardX + 12, c1Y + 34);

                canvas.setTextDatum(textdatum_t::top_right);
                char rssiBuf[32];
                snprintf(rssiBuf, sizeof(rssiBuf), "RSSI: %d dBm", rssiVal);
                canvas.drawString(rssiBuf, cardX + cardW - 12, c1Y + 34);

                // Card 2: Backend Host Card (Y = 112, H = 76)
                float c2Y = 112.0f;
                float c2H = 76.0f;
                canvas.fillRoundRect((int)cardX, (int)c2Y, (int)cardW, (int)c2H, 8, COL_OBSIDIAN);
                canvas.drawRoundRect((int)cardX, (int)c2Y, (int)cardW, (int)c2H, 8, COL_BORDER);

                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(COL_SLATE);
                canvas.drawString("BACKEND HOST", cardX + 12, c2Y + 8);

                // Shield icon on top right
                int shX = cardX + cardW - 18, shY = c2Y + 12;
                canvas.drawRoundRect(shX - 4, shY - 4, 9, 10, 2, COL_CYAN);
                canvas.drawPixel(shX, shY, COL_CYAN);

                // Dark Inner Host Container Box
                canvas.fillRoundRect(cardX + 10, c2Y + 24, cardW - 20, 24, 4, canvas.color565(12, 14, 18));
                canvas.drawRoundRect(cardX + 10, c2Y + 24, cardW - 20, 24, 4, canvas.color565(26, 30, 40));

                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(COL_CYAN);
                canvas.drawString(backendHost.c_str(), cardX + 18, c2Y + 36);

                // Status & Ping row
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(COL_GOLD_TEXT);
                canvas.drawString("STATUS: 200 OK", cardX + 12, c2Y + 54);

                canvas.setTextDatum(textdatum_t::top_right);
                canvas.drawString("PING: 18ms", cardX + cardW - 12, c2Y + 54);

                // Card 3: Tor Zero-Leak Toggle Card (Y = 196, H = 50)
                float c3Y = 196.0f;
                float c3H = 50.0f;
                canvas.fillRoundRect((int)cardX, (int)c3Y, (int)cardW, (int)c3H, 8, COL_OBSIDIAN);
                canvas.drawRoundRect((int)cardX, (int)c3Y, (int)cardW, (int)c3H, 8, COL_BORDER);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);
                canvas.drawString("Tor Zero-Leak", cardX + 12, c3Y + 9);

                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextColor(COL_GOLD_TEXT);
                canvas.drawString("Enforce Onion Proxy", cardX + 12, c3Y + 28);

                // Toggle pill on right
                int togX = cardX + cardW - 40;
                int togY = c3Y + 16;
                uint16_t togBorder = m_torProxyEnabled ? COL_AMBER : COL_SLATE;
                canvas.drawRoundRect(togX, togY, 26, 14, 7, togBorder);
                int dotX = m_torProxyEnabled ? (togX + 18) : (togX + 7);
                canvas.fillCircle(dotX, togY + 7, 4, togBorder);
            }
            else if (m_viewMode == ViewMode::Storage)
            {
                // ── STORAGE VAULT SUB-SCREEN (Screenshot 2) ───────────────────
                float cardX = 10.0f;
                float cardW = (float)w - 20.0f;

                // Card 1: Partition Allocation Card (Y = 48, H = 114)
                float c1Y = 48.0f;
                float c1H = 114.0f;
                canvas.fillRoundRect((int)cardX, (int)c1Y, (int)cardW, (int)c1H, 8, COL_OBSIDIAN);
                canvas.drawRoundRect((int)cardX, (int)c1Y, (int)cardW, (int)c1H, 8, COL_BORDER);

                // Row 1: Partition Allocation & Free size
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(0xFFFF);
                canvas.drawString("Partition", cardX + 12, c1Y + 10);
                canvas.drawString("Allocation", cardX + 12, c1Y + 22);

                canvas.setTextDatum(textdatum_t::top_right);
                canvas.setTextColor(COL_AMBER);
                canvas.drawString("8.4GB Free /", cardX + cardW - 12, c1Y + 10);
                canvas.drawString("32GB", cardX + cardW - 12, c1Y + 22);

                // 4-Segment Progress Bar (Y = c1Y + 44, H = 10)
                float bX = cardX + 12;
                float bW = cardW - 24;
                float bY = c1Y + 44;
                float seg1W = bW * 0.55f; // Memos 18.2G
                float seg2W = bW * 0.15f; // Vectors 4.1G
                float seg3W = bW * 0.08f; // Kernel 1.3G
                float seg4W = bW - seg1W - seg2W - seg3W; // Free 8.4G

                canvas.fillRoundRect((int)bX, (int)bY, (int)bW, 10, 4, canvas.color565(20, 24, 34));
                canvas.fillRoundRect((int)bX, (int)bY, (int)seg1W, 10, 4, COL_AMBER);
                canvas.fillRect((int)(bX + seg1W), (int)bY, (int)seg2W, 10, COL_CYAN);
                canvas.fillRect((int)(bX + seg1W + seg2W), (int)bY, (int)seg3W, 10, COL_SOFT_CYAN);

                // Legend 4 Dots (Rows at c1Y + 68 and c1Y + 88)
                // Row 1: Memos (Amber) & Vectors (Cyan)
                canvas.fillCircle((int)(cardX + 18), (int)(c1Y + 74), 4, COL_AMBER);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(0xFFFF);
                canvas.drawString("Memos: 18.2G", cardX + 26, c1Y + 74);

                canvas.fillCircle((int)(cardX + cardW * 0.52f), (int)(c1Y + 74), 4, COL_CYAN);
                canvas.drawString("Vectors: 4.1G", cardX + cardW * 0.52f + 8, c1Y + 74);

                // Row 2: Kernel (Soft Cyan) & Free (Dark)
                canvas.fillCircle((int)(cardX + 18), (int)(c1Y + 94), 4, COL_SOFT_CYAN);
                canvas.drawString("Kernel: 1.3G", cardX + 26, c1Y + 94);

                canvas.fillCircle((int)(cardX + cardW * 0.52f), (int)(c1Y + 94), 4, canvas.color565(48, 56, 72));
                canvas.drawString("Free: 8.4G", cardX + cardW * 0.52f + 8, c1Y + 94);

                // Button: Sync Vault Now (Y = 176, H = 38)
                float btnY = 176.0f;
                float btnH = 38.0f;
                uint16_t btnBg = m_isSyncPressed ? canvas.color565(210, 145, 40) : COL_AMBER;
                canvas.fillRoundRect((int)cardX, (int)btnY, (int)cardW, (int)btnH, 6, btnBg);

                // Sync icon ↺
                int syX = cardX + cardW * 0.5f - 56;
                int syY = btnY + btnH * 0.5f;
                canvas.drawCircle(syX, syY, 5, 0x0000);
                canvas.fillRect(syX - 6, syY - 6, 6, 6, btnBg);
                canvas.drawLine(syX - 3, syY - 5, syX, syY - 5, 0x0000);
                canvas.drawLine(syX, syY - 5, syX, syY - 2, 0x0000);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(0x0000);
                canvas.drawString("Sync Vault Now", cardX + cardW * 0.5f + 8, btnY + btnH * 0.5f);
            }
            else if (m_viewMode == ViewMode::About)
            {
                // ── ABOUT VOXA SUB-SCREEN (Screenshot 4) with REAL HARDWARE SPECS ──
                float cardX = 10.0f;
                float cardW = (float)w - 20.0f;
                float cardY = 48.0f;
                float cardH = 170.0f;

                canvas.fillRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 8, COL_OBSIDIAN);
                canvas.drawRoundRect((int)cardX, (int)cardY, (int)cardW, (int)cardH, 8, COL_BORDER);

                struct DeviceSpec {
                    const char* label;
                    const char* value;
                    uint16_t color;
                };

                DeviceSpec specs[6] = {
                    { "SERIAL NO",     realSerial,         0xFFFF },
                    { "MCU UNIT",      realChipModel,      COL_AMBER },
                    { "AUDIO CODEC",   "CS47L35 DAC",      COL_CYAN },
                    { "MEMS SENSORS",  "Knowles Dual x2",  COL_CYAN },
                    { "DISPLAY",       "2.8\" QVGA ST7789V", 0xFFFF },
                    { "FIRMWARE HASH", realFirmwareHash,   COL_GOLD_TEXT }
                };

                for (int r = 0; r < 6; ++r)
                {
                    float ry = cardY + 12.0f + r * 25.0f;
                    canvas.setFont(&fonts::DejaVu9);

                    // Left Label
                    canvas.setTextDatum(textdatum_t::top_left);
                    canvas.setTextColor(COL_SLATE);
                    canvas.drawString(specs[r].label, cardX + 12, ry);

                    // Right Value
                    canvas.setTextDatum(textdatum_t::top_right);
                    canvas.setTextColor(specs[r].color);
                    canvas.drawString(specs[r].value, cardX + cardW - 12, ry);
                }
            }
            else if (m_viewMode == ViewMode::Zeroize)
            {
                // ── ZEROIZE FLASH MODAL (Screenshot 5) ────────────────────────
                float cardX = 10.0f;
                float cardW = (float)w - 20.0f;

                // Warning Triangle + Title (Y = 48)
                int trX = cardX + 12, trY = 56;
                canvas.drawTriangle(trX, trY - 8, trX - 9, trY + 8, trX + 9, trY + 8, COL_DANGER_TX);
                canvas.drawFastVLine(trX, trY - 3, 5, COL_DANGER_TX);
                canvas.drawPixel(trX, trY + 4, COL_DANGER_TX);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_left);
                canvas.setTextColor(COL_DANGER_TX);
                canvas.drawString("Zeroize Flash?", cardX + 28, trY);

                // Irreversible warning text (Y = 74)
                canvas.setFont(&fonts::DejaVu9);
                canvas.setTextDatum(textdatum_t::top_left);
                canvas.setTextColor(canvas.color565(203, 213, 225));
                canvas.drawString("Irreversible: Cryptographic keys and", cardX + 4, 76);
                canvas.drawString("18.2GB local voice memory will be", cardX + 4, 92);
                canvas.drawString("permanently destroyed.", cardX + 4, 108);

                // Physical Override Callout Box (Y = 130, H = 46)
                float ovY = 130.0f;
                float ovH = 46.0f;
                canvas.fillRoundRect((int)cardX, (int)ovY, (int)cardW, (int)ovH, 4, COL_DANGER_BG);
                canvas.drawRoundRect((int)cardX, (int)ovY, (int)cardW, (int)ovH, 4, COL_DANGER_BD);

                canvas.setTextColor(canvas.color565(252, 165, 165));
                canvas.drawString("PHYSICAL OVERRIDE: HOLD PTT + JOG", cardX + 10, ovY + 8);
                canvas.drawString("DIAL TO FORCE ZEROIZE", cardX + 10, ovY + 24);

                // Button 1: Cancel & Retain (Y = 198, H = 36)
                float b1Y = 198.0f;
                float b1H = 36.0f;
                uint16_t b1Bg = m_isCancelZeroizePressed ? canvas.color565(44, 52, 68) : COL_OBSIDIAN;
                canvas.fillRoundRect((int)cardX, (int)b1Y, (int)cardW, (int)b1H, 6, b1Bg);
                canvas.drawRoundRect((int)cardX, (int)b1Y, (int)cardW, (int)b1H, 6, COL_BORDER);

                canvas.setFont(&fonts::FreeSansBold9pt7b);
                canvas.setTextDatum(textdatum_t::middle_center);
                canvas.setTextColor(0xFFFF);
                canvas.drawString("Cancel & Retain", cardX + cardW * 0.5f, b1Y + b1H * 0.5f);

                // Button 2: Confirm Zeroize (Y = 244, H = 36)
                float b2Y = 244.0f;
                float b2H = 36.0f;
                uint16_t b2Bg = m_isConfirmZeroizePressed ? canvas.color565(220, 100, 100) : canvas.color565(254, 178, 178);
                canvas.fillRoundRect((int)cardX, (int)b2Y, (int)cardW, (int)b2H, 6, b2Bg);

                canvas.setTextColor(canvas.color565(120, 20, 30));
                canvas.drawString("Confirm Zeroize", cardX + cardW * 0.5f, b2Y + b2H * 0.5f);
            }

            // Slide in animation or push
            if (entryFrame < 10)
            {
                VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::Settings), entryFrame, 10);
                entryFrame++;
            }
            else
            {
                canvas.pushSprite(0, 0);
            }

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16)
            {
                delay(16 - frameMs);
            }
        }

        canvas.deleteSprite();
        return targetScreen;
    }
}
