#pragma once

#include <string>

#include "../core/Widget.h"
#include "../graphics/Icons.h"

namespace VOXA
{
    class ListTile : public Widget
    {
    public:
        ListTile(Rect bounds, Icon icon, std::string title, std::string subtitle, SDL_Color iconColor, SDL_Color trailingDot, bool showChevron = true);

        void render(Renderer& renderer) const override;

    private:
        Icon m_icon;
        std::string m_title;
        std::string m_subtitle;
        SDL_Color m_iconColor;
        SDL_Color m_trailingDot;
        bool m_showChevron;
    };
}
