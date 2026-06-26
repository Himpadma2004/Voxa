#include "SettingsScreen.h"

#include <array>

#include "../core/Application.h"
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

        // Tap on "Sync & Backup" tile (index 1, y = 285.0f)
        if (Rect { 380.0f, 285.0f, 840.0f, 80.0f }.contains(point.x, point.y))
        {
            app.navigateTo(ScreenId::SyncStatus);
        }
    }

    void SettingsScreen::update(Application&, float)
    {
    }

    void SettingsScreen::render(Application&, Renderer& renderer)
    {
        ScreenCommon::renderSurface(renderer);
        ScreenCommon::renderHeader(renderer, "Settings", true, true, Icon::Plus);

        // Center glass card container
        Card container(Rect { 300.0f, 140.0f, 1000.0f, 640.0f }, Colors::Card, 32.0f);
        container.setShadow(Colors::Shadow, 8);
        container.setBorder(Colors::GlassBorder);
        container.render(renderer);

        const std::array<SettingItem, 5> items { {
            { Icon::Wifi, "Wi-Fi", "Connected", SDL_Color { 68, 162, 255, 255 }, ScreenId::Settings },
            { Icon::Cloud, "Sync & Backup", "Auto sync on", SDL_Color { 68, 162, 255, 255 }, ScreenId::SyncStatus },
            { Icon::Storage, "Storage", "12.4 GB / 32 GB", SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Info, "Device Info", "VOXA V1.0", SDL_Color { 80, 80, 88, 255 }, ScreenId::Settings },
            { Icon::Star, "About VOXA", "Personal AI Assistant", Colors::Primary, ScreenId::Settings },
        } };

        for (std::size_t i = 0; i < items.size(); ++i)
        {
            ListTile tile(Rect { 380.0f, 190.0f + i * 95.0f, 840.0f, 80.0f }, items[i].icon, items[i].title, items[i].subtitle, items[i].color, SDL_Color { 0, 0, 0, 0 }, true);
            tile.render(renderer);
        }
    }
}
