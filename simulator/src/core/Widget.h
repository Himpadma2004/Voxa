#pragma once

#include <SDL3/SDL.h>

namespace VOXA
{
    class Renderer;

    struct Rect
    {
        float x {};
        float y {};
        float w {};
        float h {};

        [[nodiscard]] bool contains(float px, float py) const
        {
            return px >= x && px <= (x + w) && py >= y && py <= (y + h);
        }

        [[nodiscard]] SDL_FRect toSDL() const
        {
            return SDL_FRect { x, y, w, h };
        }
    };

    class Widget
    {
    public:
        explicit Widget(Rect bounds)
            : m_bounds(bounds)
        {
        }

        virtual ~Widget() = default;

        [[nodiscard]] const Rect& bounds() const
        {
            return m_bounds;
        }

        [[nodiscard]] bool hitTest(float px, float py) const
        {
            return m_bounds.contains(px, py);
        }

        virtual void render(Renderer& renderer) const = 0;

    protected:
        Rect m_bounds;
    };
}
