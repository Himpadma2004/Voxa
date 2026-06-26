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

        switch (icon)
        {
        case Icon::Mic:
            renderer.drawRoundedRect(x + size * 0.34f, y + size * 0.14f, size * 0.32f, size * 0.46f, size * 0.16f, color);
            renderer.drawLine(cx, y + size * 0.62f, cx, y + size * 0.82f, color);
            renderer.drawLine(x + size * 0.28f, y + size * 0.5f, x + size * 0.72f, y + size * 0.5f, color);
            renderer.drawLine(x + size * 0.38f, y + size * 0.82f, x + size * 0.62f, y + size * 0.82f, color);
            break;
        case Icon::Bell:
            renderer.drawLine(cx, y + size * 0.16f, cx, y + size * 0.2f, color);
            renderer.drawRoundedRect(x + size * 0.26f, y + size * 0.22f, size * 0.48f, size * 0.42f, size * 0.22f, color);
            renderer.drawLine(x + size * 0.24f, y + size * 0.64f, x + size * 0.76f, y + size * 0.64f, color);
            renderer.drawCircle(cx, y + size * 0.74f, size * 0.06f, color);
            break;
        case Icon::Lightbulb:
            renderer.drawCircle(cx, y + size * 0.34f, size * 0.2f, color);
            renderer.drawLine(x + size * 0.42f, y + size * 0.56f, x + size * 0.58f, y + size * 0.56f, color);
            renderer.drawLine(x + size * 0.44f, y + size * 0.66f, x + size * 0.56f, y + size * 0.66f, color);
            renderer.drawLine(cx, y + size * 0.54f, cx, y + size * 0.66f, color);
            break;
        case Icon::Question:
            renderer.drawTextCentered("?", cx, y + size * 0.15f, color, static_cast<int>(size * 0.9f));
            break;
        case Icon::Search:
            renderer.drawCircle(x + size * 0.42f, y + size * 0.42f, size * 0.2f, color);
            renderer.drawLine(x + size * 0.56f, y + size * 0.56f, x + size * 0.78f, y + size * 0.78f, color);
            break;
        case Icon::Folder:
            renderer.drawRoundedRect(x + size * 0.18f, y + size * 0.28f, size * 0.64f, size * 0.42f, size * 0.08f, color);
            renderer.drawLine(x + size * 0.18f, y + size * 0.36f, x + size * 0.38f, y + size * 0.36f, color);
            renderer.drawLine(x + size * 0.36f, y + size * 0.22f, x + size * 0.52f, y + size * 0.28f, color);
            break;
        case Icon::Settings:
            drawGear(renderer, x, y, size, color);
            break;
        case Icon::ChevronRight:
            drawChevron(renderer, x, y, size, color, false);
            break;
        case Icon::Back:
            drawChevron(renderer, x, y, size, color, true);
            break;
        case Icon::Plus:
            renderer.drawLine(cx, y + size * 0.22f, cx, y + size * 0.78f, color);
            renderer.drawLine(x + size * 0.22f, cy, x + size * 0.78f, cy, color);
            break;
        case Icon::Filter:
            renderer.drawLine(x + size * 0.24f, y + size * 0.24f, x + size * 0.76f, y + size * 0.24f, color);
            renderer.drawLine(x + size * 0.34f, y + size * 0.44f, x + size * 0.66f, y + size * 0.44f, color);
            renderer.drawLine(x + size * 0.44f, y + size * 0.66f, x + size * 0.56f, y + size * 0.66f, color);
            break;
        case Icon::Wifi:
            renderer.drawLine(x + size * 0.24f, y + size * 0.44f, x + size * 0.5f, y + size * 0.26f, color);
            renderer.drawLine(x + size * 0.5f, y + size * 0.26f, x + size * 0.76f, y + size * 0.44f, color);
            renderer.drawLine(x + size * 0.34f, y + size * 0.56f, x + size * 0.5f, y + size * 0.42f, color);
            renderer.drawLine(x + size * 0.5f, y + size * 0.42f, x + size * 0.66f, y + size * 0.56f, color);
            renderer.fillCircle(cx, y + size * 0.7f, size * 0.05f, color);
            break;
        case Icon::Battery:
            renderer.drawRoundedRect(x + size * 0.18f, y + size * 0.28f, size * 0.56f, size * 0.3f, size * 0.06f, color);
            renderer.fillRect(x + size * 0.76f, y + size * 0.36f, size * 0.06f, size * 0.14f, color);
            renderer.fillRect(x + size * 0.24f, y + size * 0.34f, size * 0.34f, size * 0.18f, color);
            break;
        case Icon::Calendar:
            renderer.drawRoundedRect(x + size * 0.22f, y + size * 0.24f, size * 0.56f, size * 0.54f, size * 0.08f, color);
            renderer.drawLine(x + size * 0.22f, y + size * 0.38f, x + size * 0.78f, y + size * 0.38f, color);
            renderer.drawLine(x + size * 0.34f, y + size * 0.18f, x + size * 0.34f, y + size * 0.32f, color);
            renderer.drawLine(x + size * 0.66f, y + size * 0.18f, x + size * 0.66f, y + size * 0.32f, color);
            break;
        case Icon::Cloud:
            renderer.drawCircle(x + size * 0.42f, y + size * 0.5f, size * 0.18f, color);
            renderer.drawCircle(x + size * 0.58f, y + size * 0.44f, size * 0.16f, color);
            renderer.drawCircle(x + size * 0.7f, y + size * 0.52f, size * 0.12f, color);
            renderer.drawLine(x + size * 0.28f, y + size * 0.62f, x + size * 0.76f, y + size * 0.62f, color);
            break;
        case Icon::Storage:
            renderer.drawRoundedRect(x + size * 0.2f, y + size * 0.26f, size * 0.6f, size * 0.48f, size * 0.08f, color);
            renderer.drawLine(x + size * 0.28f, y + size * 0.44f, x + size * 0.72f, y + size * 0.44f, color);
            renderer.fillCircle(x + size * 0.32f, y + size * 0.58f, size * 0.04f, color);
            renderer.fillCircle(x + size * 0.44f, y + size * 0.58f, size * 0.04f, color);
            break;
        case Icon::Info:
            renderer.drawCircle(cx, cy, size * 0.3f, color);
            renderer.drawLine(cx, y + size * 0.42f, cx, y + size * 0.62f, color);
            renderer.fillCircle(cx, y + size * 0.3f, size * 0.04f, color);
            break;
        case Icon::Star:
            renderer.drawTextCentered("*", cx, y + size * 0.12f, color, static_cast<int>(size));
            break;
        case Icon::Note:
            renderer.drawRoundedRect(x + size * 0.24f, y + size * 0.18f, size * 0.5f, size * 0.6f, size * 0.08f, color);
            renderer.drawLine(x + size * 0.34f, y + size * 0.36f, x + size * 0.62f, y + size * 0.36f, color);
            renderer.drawLine(x + size * 0.34f, y + size * 0.5f, x + size * 0.6f, y + size * 0.5f, color);
            break;
        case Icon::Chat:
            renderer.drawRoundedRect(x + size * 0.2f, y + size * 0.22f, size * 0.6f, size * 0.44f, size * 0.1f, color);
            renderer.drawLine(x + size * 0.38f, y + size * 0.66f, x + size * 0.3f, y + size * 0.8f, color);
            break;
        case Icon::Spark:
            renderer.drawLine(cx, y + size * 0.16f, cx, y + size * 0.84f, color);
            renderer.drawLine(x + size * 0.16f, cy, x + size * 0.84f, cy, color);
            renderer.drawLine(x + size * 0.28f, y + size * 0.28f, x + size * 0.72f, y + size * 0.72f, color);
            renderer.drawLine(x + size * 0.72f, y + size * 0.28f, x + size * 0.28f, y + size * 0.72f, color);
            break;
        case Icon::Upload:
            renderer.drawCircle(cx, cy, size * 0.38f, color);
            renderer.drawLine(cx, y + size * 0.68f, cx, y + size * 0.34f, color);
            renderer.drawLine(cx, y + size * 0.34f, x + size * 0.38f, y + size * 0.48f, color);
            renderer.drawLine(cx, y + size * 0.34f, x + size * 0.62f, y + size * 0.48f, color);
            break;
        }
    }
}
