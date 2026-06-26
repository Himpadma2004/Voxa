#include "SettingsScreen.h"

#include <array>

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
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            return;
        }

        const SDL_FPoint point = app.windowToCanvas(event.button.x, event.button.y);
        if (Rect { 44.0f, 34.0f, 56.0f, 56.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::Home);
            return;
        }

        // Tap on "Wi-Fi" tile (index 0, y = 190.0f)
        if (Rect { 380.0f, 190.0f, 840.0f, 80.0f }.contains(point.x, point.y))
        {
            if (app.services().settings)
            {
                auto settings = app.services().settings->getSettings();
                settings.wifiEnabled = !settings.wifiEnabled;
                app.services().settings->updateSettings(settings);
                app.audio().playSoftConfirm();
            }
            return;
        }

        // Tap on "Sync & Backup" tile (index 1, y = 285.0f)
        if (Rect { 380.0f, 285.0f, 840.0f, 80.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::SyncStatus);
        }
    }

    void SettingsScreen::update(Application&, float)
    {
    }

    void SettingsScreen::render(Application& app, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Settings", true, true, Icon::Plus);

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        Settings settings;
        if (app.services().settings)
        {
            settings = app.services().settings->getSettings();
        }

        // storage limit subtitle format
        char storageText[64] = "12.4 GB / 32 GB";
        if (settings.storageLimit > 0)
        {
            float limitGB = static_cast<float>(settings.storageLimit) / 1024.0f;
            SDL_snprintf(storageText, sizeof(storageText), "12.4 GB / %.1f GB", limitGB);
        }

        // Device info subtitle format
        char deviceInfoText[128] = "VOXA Device (v1.0.0)";
        if (!settings.deviceName.empty())
        {
            SDL_snprintf(deviceInfoText, sizeof(deviceInfoText), "%s (v%s)", settings.deviceName.c_str(), settings.firmwareVersion.c_str());
        }

        const std::array<SettingItem, 5> items { {
            { Icon::Wifi, "Wi-Fi", settings.wifiEnabled ? "Connected" : "Disconnected", settings.wifiEnabled ? SDL_Color { 68, 192, 122, 255 } : SDL_Color { 180, 80, 80, 255 }, ScreenId::Settings },
            { Icon::Cloud, "Sync & Backup", settings.autoSync ? "Auto sync on" : "Auto sync off", SDL_Color { 68, 162, 255, 255 }, ScreenId::SyncStatus },
            { Icon::Storage, "Storage", storageText, SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Info, "Device Info", deviceInfoText, SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Star, "About VOXA", "Personal AI Assistant", Colors::Primary, ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 380.0f, 190.0f + i * 95.0f, 840.0f, 80.0f }, items[i].icon, items[i].title, items[i].subtitle, items[i].color, SDL_Color { 0, 0, 0, 0 }, true);
            tile.render(renderer);
        }
    }
}
