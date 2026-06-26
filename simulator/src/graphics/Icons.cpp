#include "Icons.h"

#include <cmath>

#include "Renderer.h"

namespace
{
    void drawChevron(VOXA::Renderer& renderer, float x, float y, float size, SDL_Color color, bool left)
    {
        const float midY = y + size * 0.5f;
        if (left)
        {
            renderer.drawLine(x + size * 0.62f, y + size * 0.22f, x + size * 0.36f, midY, color);
            renderer.drawLine(x + size * 0.36f, midY, x + size * 0.62f, y + size * 0.78f, color);
        }
        else
        {
            renderer.drawLine(x + size * 0.38f, y + size * 0.22f, x + size * 0.64f, midY, color);
            renderer.drawLine(x + size * 0.64f, midY, x + size * 0.38f, y + size * 0.78f, color);
        }
    }

    void drawGear(VOXA::Renderer& renderer, float x, float y, float size, SDL_Color color)
    {
        const float cx = x + size * 0.5f;
        const float cy = y + size * 0.5f;
        renderer.drawCircle(cx, cy, size * 0.24f, color);
        renderer.drawCircle(cx, cy, size * 0.08f, color);
        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * 0.78539816f;
            const float x0 = cx + std::cos(angle) * size * 0.28f;
            const float y0 = cy + std::sin(angle) * size * 0.28f;
            const float x1 = cx + std::cos(angle) * size * 0.4f;
            const float y1 = cy + std::sin(angle) * size * 0.4f;
            renderer.drawLine(x0, y0, x1, y1, color);
        }
    }
}

namespace VOXA
{
    void drawIcon(Renderer& renderer, Icon icon, float x, float y, float size, SDL_Color color)
    {
        const float cx = x + size * 0.5f;
        const float cy = y + size * 0.5f;

        char32_t codepoint = 0;
        switch (icon)
        {
        case Icon::Mic:          codepoint = 0xE720; break;
        case Icon::Bell:         codepoint = 0xE730; break;
        case Icon::Lightbulb:    codepoint = 0xEA80; break;
        case Icon::Question:     codepoint = 0xE897; break;
        case Icon::Search:       codepoint = 0xE721; break;
        case Icon::Folder:       codepoint = 0xE8B7; break;
        case Icon::Settings:     codepoint = 0xE713; break;
        case Icon::ChevronRight: codepoint = 0xE76C; break;
        case Icon::Back:         codepoint = 0xE76B; break;
        case Icon::Plus:         codepoint = 0xE710; break;
        case Icon::Filter:       codepoint = 0xE71C; break;
        case Icon::Wifi:         codepoint = 0xE701; break;
        case Icon::Battery:      codepoint = 0xE859; break;
        case Icon::Calendar:     codepoint = 0xE787; break;
        case Icon::Cloud:        codepoint = 0xE753; break;
        case Icon::Storage:      codepoint = 0xE9A5; break; // HardDrive
        case Icon::Info:         codepoint = 0xE946; break;
        case Icon::Star:         codepoint = 0xE734; break;
        case Icon::Note:         codepoint = 0xE70B; break;
        case Icon::Chat:         codepoint = 0xE8F2; break;
        case Icon::Spark:        codepoint = 0xEA8A; break; // ActionCenter/Sparkle
        case Icon::Upload:       codepoint = 0xE898; break; // Upload
        }

        if (codepoint != 0)
        {
            renderer.drawIconGlyph(codepoint, cx, cy, color, static_cast<int>(size));
        }
    }
}
