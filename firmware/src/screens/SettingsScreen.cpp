#include "SettingsScreen.h"

#include <array>
#include <cmath>
#include <algorithm>

#include "../core/Application.h"
#include "../core/services/SettingsService.h"
#include "../core/models/Settings.h"
#include "../graphics/Colors.h"
#include "../graphics/Renderer.h"
#include "../widgets/Card.h"
#include "../widgets/ListTile.h"
#include "ScreenCommon.h"

namespace
{
    struct SettingItem
    {
        VOXA::Icon icon;
        const char* title;
        const char* subtitle;
        SDL_Color color;
        VOXA::ScreenId target;
    };
}

namespace VOXA
{
    ScreenId SettingsScreen::id() const
    {
        return ScreenId::Settings;
    }

    void SettingsScreen::handleEvent(Application& app, const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
            // Header back button hit area centered at (18, 28) with radius 11
            if (Rect { 0.0f, 0.0f, 40.0f, 40.0f }.contains(point.x, point.y))
            {
                app.navigateBack();
                return;
            }

            if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(point.x, point.y))
            {
                m_isDragging = true;
                m_dragStartY = point.y;
                m_dragStartScrollY = m_targetScrollY;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.motion.x, event.motion.y);
                float diffY = point.y - m_dragStartY;
                m_targetScrollY = m_dragStartScrollY - diffY;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (m_isDragging)
            {
                const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
                float diffY = std::abs(point.y - m_dragStartY);
                if (diffY < 6.0f)
                {
                    // Wi-Fi tile (index 0, y = 50.0f)
                    Rect wifiRect { 10.0f, 50.0f - m_scrollY, 300.0f, 48.0f };
                    if (wifiRect.contains(point.x, point.y) && point.y >= 48.0f && point.y <= 230.0f)
                    {
                        if (app.services().settings)
                        {
                            auto settings = app.services().settings->getSettings();
                            settings.wifiEnabled = !settings.wifiEnabled;
                            app.services().settings->updateSettings(settings);
                            app.audio().playSoftConfirm();
                        }
                        m_isDragging = false;
                        return;
                    }
 
                    // Sync & Backup tile (index 1, y = 104.0f)
                    Rect syncRect { 10.0f, 104.0f - m_scrollY, 300.0f, 48.0f };
                    if (syncRect.contains(point.x, point.y) && point.y >= 48.0f && point.y <= 230.0f)
                    {
                        app.navigateTo(ScreenId::SyncStatus);
                        m_isDragging = false;
                        return;
                    }
                }
                m_isDragging = false;
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            float mx = 0.0f, my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            const SDL_FPoint mPt = app.windowToCanvas(mx, my);
            if (Rect { 10.0f, 48.0f, 300.0f, 182.0f }.contains(mPt.x, mPt.y))
            {
                m_targetScrollY -= event.wheel.y * 20.0f;
            }
        }
    }

    void SettingsScreen::update(Application& app, float deltaSeconds)
    {
        float contentHeight = 5.0f * 54.0f;
        float visibleHeight = 182.0f;
        float maxScrollY = std::max(0.0f, contentHeight - visibleHeight);

        m_targetScrollY = std::clamp(m_targetScrollY, 0.0f, maxScrollY);
        m_scrollY += (m_targetScrollY - m_scrollY) * 12.0f * deltaSeconds;
        if (std::abs(m_targetScrollY - m_scrollY) < 0.1f)
        {
            m_scrollY = m_targetScrollY;
        }
    }

    void SettingsScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Settings", true, false, Icon::Plus);

        Settings settings;
        if (app.services().settings)
        {
            settings = app.services().settings->getSettings();
        }

        char storageText[64] = "12.4 GB / 32 GB";
        if (settings.storageLimit > 0)
        {
            float limitGB = static_cast<float>(settings.storageLimit) / 1024.0f;
            SDL_snprintf(storageText, sizeof(storageText), "12.4 GB / %.1f GB", limitGB);
        }

        char deviceInfoText[128] = "VOXA (v1.0.0)";
        if (!settings.deviceName.empty())
        {
            SDL_snprintf(deviceInfoText, sizeof(deviceInfoText), "%s (v%s)", settings.deviceName.c_str(), settings.firmwareVersion.c_str());
        }

        const std::array<SettingItem, 5> items { {
            { Icon::Wifi, "Wi-Fi", settings.wifiEnabled ? "Connected" : "Disconnected", settings.wifiEnabled ? SDL_Color { 68, 192, 122, 255 } : SDL_Color { 180, 80, 80, 255 }, ScreenId::Settings },
            { Icon::Cloud, "Sync & Backup", settings.autoSync ? "Auto sync on" : "Auto sync off", SDL_Color { 68, 162, 255, 255 }, ScreenId::SyncStatus },
            { Icon::Storage, "Storage", storageText, SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Info, "Device Info", deviceInfoText, SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Star, "About VOXA", "AI Companion", Colors::Primary, ScreenId::Settings },
        } };

        renderer.setClipRect(5.0f, 48.0f, 310.0f, 182.0f);

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 10.0f, 50.0f + i * 54.0f - m_scrollY, 300.0f, 48.0f }, 
                           items[i].icon, 
                           items[i].title, 
                           items[i].subtitle, 
                           items[i].color, 
                           SDL_Color { 0, 0, 0, 0 }, 
                           true);
            tile.render(renderer);
        }

        renderer.clearClipRect();
    }
}
