#include "BluetoothSettingsScreen.h"
#include "../services/BluetoothManager.h"
#include "../display/Display.h"

#include "../ui/Theme.h"
#include "Transition.h"
#include <algorithm>
#include <cmath>

namespace VOXA
{
    ScreenId BluetoothSettingsScreen::show(Touch& touch)
    {
        uint16_t w = Display::width();
        uint16_t h = Display::height();

        LGFX_Sprite canvas(&Display::lcd);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        bool useSprite = canvas.createSprite(w, h);
        if (useSprite) canvas.fillScreen(0);
        LovyanGFX& target = useSprite ? (LovyanGFX&)canvas : (LovyanGFX&)Display::lcd;

        if (!m_hasScanned)
        {
            performScan();
        }

        ScreenId targetScreen = ScreenId::BluetoothSettings;
        uint32_t lastMs = millis();
        int entryFrame = 0;

        while (targetScreen == ScreenId::BluetoothSettings)
        {
            uint32_t nowMs = millis();
            float deltaSecs = (nowMs - lastMs) / 1000.0f;
            lastMs = nowMs;

            uint16_t tx = 0, ty = 0;
            bool touched = touch.getPoint(tx, ty);

            float contentHeight = 80.0f + m_devices.size() * 50.0f;
            float visibleHeight = h - 70.0f - 18.0f;
            float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

            if (touched)
            {
                if (!m_wasTouched)
                {
                    m_wasTouched = true;
                    m_dragStartY = ty;
                    m_lastDragY = ty;
                    m_lastTouchMs = nowMs;
                    m_isDragging = false;
                    m_pressedIndex = -1;
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;

                    if (ty <= 65 && tx <= 50)
                    {
                        m_isBackPressed = true;
                    }
                    else if (ty <= 65 && tx >= w - 50)
                    {
                        m_isRefreshPressed = true;
                    }
                    else if (ty >= 70 && ty <= h - 18)
                    {
                        // Check tap on Bluetooth enable toggle row
                        if (ty >= 75 - m_scrollY && ty <= 115 - m_scrollY)
                        {
                            m_pressedIndex = 99; // 99 = toggle
                        }
                        else
                        {
                            int idx = static_cast<int>((ty - (125.0f - m_scrollY)) / 50.0f);
                            if (idx >= 0 && idx < static_cast<int>(m_devices.size()))
                            {
                                m_pressedIndex = idx;
                            }
                        }
                    }
                }
                else
                {
                    float dy = ty - m_lastDragY;
                    if (std::abs(ty - m_dragStartY) > 8.0f)
                    {
                        m_isDragging = true;
                        m_pressedIndex = -1;
                    }

                    if (m_isDragging)
                    {
                        m_targetScrollY -= dy;
                        m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
                        if (nowMs > m_lastTouchMs)
                        {
                            m_scrollVelocity = -dy / ((nowMs - m_lastTouchMs) / 1000.0f);
                        }
                    }
                    m_lastDragY = ty;
                    m_lastTouchMs = nowMs;
                }
            }
            else
            {
                if (m_wasTouched)
                {
                    m_wasTouched = false;
                    if (!m_isDragging)
                    {
                        if (m_isBackPressed)
                        {
                            targetScreen = ScreenId::Home;
                        }
                        else if (m_isRefreshPressed)
                        {
                            performScan();
                        }
                        else if (m_pressedIndex >= 0 && m_pressedIndex < static_cast<int>(m_devices.size()))
                        {
                            const auto& dev = m_devices[m_pressedIndex];
                            if (dev.isConnected)
                            {
                                BluetoothManager::instance().disconnectCurrent();
                            }
                            else
                            {
                                BluetoothManager::instance().connectToDevice(dev.address);
                            }
                            performScan(); // Refresh connection status
                        }
                    }
                    m_isBackPressed = false;
                    m_isRefreshPressed = false;
                    m_pressedIndex = -1;
                }
            }


            if (!m_wasTouched && std::abs(m_scrollVelocity) > 0.0f)
            {
                m_targetScrollY += m_scrollVelocity * deltaSecs;
                m_scrollVelocity *= std::pow(0.85f, deltaSecs * 60.0f);
                if (std::abs(m_scrollVelocity) < 5.0f) m_scrollVelocity = 0.0f;
            }
            m_targetScrollY = std::max(0.0f, std::min(maxScrollY, m_targetScrollY));
            m_scrollY += (m_targetScrollY - m_scrollY) * 15.0f * deltaSecs;

            // Render Screen
            ScreenCommon::renderSurface(target, w, h);
            ScreenCommon::renderHeader(target, "Bluetooth", true, true, Icon::Rotate, w, h);

            target.setClipRect(0, 70, w, h - 70 - 18);

            // 1. Bluetooth Toggle Card
            float toggleCardY = 75.0f - m_scrollY;
            if (toggleCardY + 44.0f >= 70.0f && toggleCardY <= h - 18.0f)
            {
                uint16_t cardBg = (m_pressedIndex == 99) ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                target.fillRoundRect(10, (int)toggleCardY, w - 20, 42, 8, cardBg);
                target.drawRoundRect(10, (int)toggleCardY, w - 20, 42, 8, VoxaTheme::getDivider());

                ScreenCommon::drawIcon(target, Icon::Bluetooth, 20, toggleCardY + 11.0f, 20.0f, VoxaTheme::getPrimary());

                target.setFont(&fonts::FreeSansBold9pt7b);
                target.setTextDatum(textdatum_t::middle_left);
                target.setTextColor(VoxaTheme::getTextPrimary());
                target.drawString("Bluetooth Status", 50, toggleCardY + 21.0f);

                // Switch Pill
                uint16_t pillBg = m_btEnabled ? VoxaTheme::getPrimary() : VoxaTheme::getDivider();
                target.fillRoundRect(w - 60, (int)toggleCardY + 11, 40, 20, 10, pillBg);
                int knobX = m_btEnabled ? (w - 30) : (w - 50);
                target.fillCircle(knobX, (int)toggleCardY + 21, 8, VoxaTheme::getTextPrimary());
            }

            // 2. Available Devices List
            if (m_btEnabled)
            {
                for (size_t i = 0; i < m_devices.size(); ++i)
                {
                    float cardY = 125.0f + i * 50.0f - m_scrollY;
                    if (cardY + 44.0f < 70.0f || cardY > h - 18.0f) continue;

                    bool isPressed = (m_pressedIndex == static_cast<int>(i));
                    uint16_t cardBg = isPressed ? VoxaTheme::getPrimary() : VoxaTheme::getSurface();
                    target.fillRoundRect(10, (int)cardY, w - 20, 44, 8, cardBg);
                    target.drawRoundRect(10, (int)cardY, w - 20, 44, 8, VoxaTheme::getDivider());

                    ScreenCommon::drawIcon(target, Icon::Bluetooth, 20, cardY + 12.0f, 20.0f, VoxaTheme::getTextPrimary());

                    target.setFont(&fonts::FreeSans9pt7b);
                    target.setTextDatum(textdatum_t::middle_left);
                    target.setTextColor(VoxaTheme::getTextPrimary());
                    target.drawString(m_devices[i].name.c_str(), 50, cardY + 14.0f);

                    target.setFont(&fonts::Font0);
                    target.setTextColor(VoxaTheme::getTextSecondary());
                    target.drawString(m_devices[i].address.c_str(), 50, cardY + 30.0f);

                    if (m_devices[i].isConnected)
                    {
                        target.setFont(&fonts::Font0);
                        target.setTextColor(VoxaTheme::getPrimary());
                        target.setTextDatum(textdatum_t::middle_right);
                        target.drawString("Connected", w - 20, cardY + 22.0f);
                    }
                }
            }

            target.clearClipRect();

            if (useSprite)
            {
                if (entryFrame < 10)
                {
                    VOXA::playSlideInFrame(canvas, VOXA::getTransitionType(VOXA::g_lastScreenId, ScreenId::BluetoothSettings), entryFrame, 10);
                    entryFrame++;
                }
                else
                {
                    canvas.pushSprite(0, 0);
                }
            }

            uint32_t frameMs = millis() - nowMs;
            if (frameMs < 16) delay(16 - frameMs);
        }

        if (useSprite) canvas.deleteSprite();
        return targetScreen;
    }

    void BluetoothSettingsScreen::performScan()

    {
        m_devices.clear();
        BluetoothManager::instance().begin();
        BluetoothManager::instance().startScan(3);

        const auto& found = BluetoothManager::instance().getDiscoveredDevices();
        for (const auto& dev : found)
        {
            BluetoothDevice bd;
            bd.name = dev.name;
            bd.address = dev.address;
            bd.rssi = dev.rssi;
            bd.isConnected = dev.isConnected;
            m_devices.push_back(bd);
        }

        m_hasScanned = true;
    }
}

