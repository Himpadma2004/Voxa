#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    SDL_Color mixColor(SDL_Color a, SDL_Color b, float t)
    {
        const float inv = 1.0f - t;
        return SDL_Color {
            static_cast<Uint8>(a.r * inv + b.r * t),
            static_cast<Uint8>(a.g * inv + b.g * t),
            static_cast<Uint8>(a.b * inv + b.b * t),
            static_cast<Uint8>(a.a * inv + b.a * t)
        };
    }
}

namespace VOXA
{
    Renderer::Renderer() = default;

    Renderer::~Renderer()
    {
        shutdown();
    }

    bool Renderer::initialize(const char* title, int windowWidth, int windowHeight, int canvasWidth, int canvasHeight)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        {
            return false;
        }

        m_window = SDL_CreateWindow(title, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (m_window == nullptr)
        {
            SDL_Quit();
            return false;
        }

        m_renderer = SDL_CreateRenderer(m_window, nullptr);
        if (m_renderer == nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            SDL_Quit();
            return false;
        }

        m_canvasWidth = canvasWidth;
        m_canvasHeight = canvasHeight;

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderLogicalPresentation(m_renderer, canvasWidth, canvasHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        m_running = true;
        return true;
    }

    void Renderer::shutdown()
    {
        m_running = false;

        if (m_renderer != nullptr)
        {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }

        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
    }

    void Renderer::beginFrame()
    {
    }

    void Renderer::endFrame()
    {
        SDL_RenderPresent(m_renderer);
    }

    bool Renderer::isRunning() const
    {
        return m_running;
    }

    SDL_Renderer* Renderer::sdlRenderer() const
    {
        return m_renderer;
    }

    int Renderer::canvasWidth() const
    {
        return m_canvasWidth;
    }

    int Renderer::canvasHeight() const
    {
        return m_canvasHeight;
    }

    float Renderer::scale() const
    {
        return 1.0f;
    }

    SDL_FPoint Renderer::windowToCanvas(float windowX, float windowY) const
    {
        SDL_FPoint point {};
        SDL_RenderCoordinatesFromWindow(m_renderer, windowX, windowY, &point.x, &point.y);
        return point;
    }

    void Renderer::clear(SDL_Color color)
    {
        setDrawColor(color);
        SDL_RenderClear(m_renderer);
    }

    void Renderer::setDrawColor(SDL_Color color)
    {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    }

    void Renderer::drawLine(float x1, float y1, float x2, float y2, SDL_Color color)
    {
        setDrawColor(color);
        SDL_RenderLine(m_renderer, x1, y1, x2, y2);
    }

    void Renderer::drawRect(float x, float y, float w, float h, SDL_Color color)
    {
        setDrawColor(color);
        const SDL_FRect rect { x, y, w, h };
        SDL_RenderRect(m_renderer, &rect);
    }

    void Renderer::fillRect(float x, float y, float w, float h, SDL_Color color)
    {
        setDrawColor(color);
        const SDL_FRect rect { x, y, w, h };
        SDL_RenderFillRect(m_renderer, &rect);
    }

    void Renderer::drawRoundedRect(float x, float y, float w, float h, float radius, SDL_Color color)
    {
        drawLine(x + radius, y, x + w - radius, y, color);
        drawLine(x + radius, y + h, x + w - radius, y + h, color);
        drawLine(x, y + radius, x, y + h - radius, color);
        drawLine(x + w, y + radius, x + w, y + h - radius, color);

        const int segments = 18;
        for (int i = 0; i < segments; ++i)
        {
            const float a0 = static_cast<float>(i) / segments;
            const float a1 = static_cast<float>(i + 1) / segments;

            const float tl0 = kPi + a0 * kPi * 0.5f;
            const float tl1 = kPi + a1 * kPi * 0.5f;
            drawLine(x + radius + std::cos(tl0) * radius, y + radius + std::sin(tl0) * radius,
                     x + radius + std::cos(tl1) * radius, y + radius + std::sin(tl1) * radius, color);

            const float tr0 = -kPi * 0.5f + a0 * kPi * 0.5f;
            const float tr1 = -kPi * 0.5f + a1 * kPi * 0.5f;
            drawLine(x + w - radius + std::cos(tr0) * radius, y + radius + std::sin(tr0) * radius,
                     x + w - radius + std::cos(tr1) * radius, y + radius + std::sin(tr1) * radius, color);

            const float br0 = a0 * kPi * 0.5f;
            const float br1 = a1 * kPi * 0.5f;
            drawLine(x + w - radius + std::cos(br0) * radius, y + h - radius + std::sin(br0) * radius,
                     x + w - radius + std::cos(br1) * radius, y + h - radius + std::sin(br1) * radius, color);

            const float bl0 = kPi * 0.5f + a0 * kPi * 0.5f;
            const float bl1 = kPi * 0.5f + a1 * kPi * 0.5f;
            drawLine(x + radius + std::cos(bl0) * radius, y + h - radius + std::sin(bl0) * radius,
                     x + radius + std::cos(bl1) * radius, y + h - radius + std::sin(bl1) * radius, color);
        }
    }

    void Renderer::fillRoundedRect(float x, float y, float w, float h, float radius, SDL_Color color)
    {
        fillRect(x + radius, y, w - radius * 2.0f, h, color);
        fillRect(x, y + radius, radius, h - radius * 2.0f, color);
        fillRect(x + w - radius, y + radius, radius, h - radius * 2.0f, color);
        fillCircle(x + radius, y + radius, radius, color);
        fillCircle(x + w - radius, y + radius, radius, color);
        fillCircle(x + w - radius, y + h - radius, radius, color);
        fillCircle(x + radius, y + h - radius, radius, color);
    }

    void Renderer::fillVerticalGradient(float x, float y, float w, float h, SDL_Color top, SDL_Color bottom)
    {
        const int lines = std::max(1, static_cast<int>(h));
        for (int i = 0; i < lines; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(lines);
            drawLine(x, y + static_cast<float>(i), x + w, y + static_cast<float>(i), mixColor(top, bottom, t));
        }
    }

    void Renderer::fillCircle(float centerX, float centerY, float radius, SDL_Color color)
    {
        setDrawColor(color);
        const int ir = static_cast<int>(radius);
        for (int y = -ir; y <= ir; ++y)
        {
            const float span = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(y * y)));
            SDL_RenderLine(m_renderer, centerX - span, centerY + static_cast<float>(y), centerX + span, centerY + static_cast<float>(y));
        }
    }

    void Renderer::drawCircle(float centerX, float centerY, float radius, SDL_Color color)
    {
        const int segments = 48;
        for (int i = 0; i < segments; ++i)
        {
            const float a0 = (static_cast<float>(i) / segments) * kPi * 2.0f;
            const float a1 = (static_cast<float>(i + 1) / segments) * kPi * 2.0f;
            drawLine(centerX + std::cos(a0) * radius, centerY + std::sin(a0) * radius,
                     centerX + std::cos(a1) * radius, centerY + std::sin(a1) * radius, color);
        }
    }

    void Renderer::fillCircleGradient(float centerX, float centerY, float radius, SDL_Color inner, SDL_Color outer)
    {
        for (int i = static_cast<int>(radius); i >= 1; --i)
        {
            const float t = 1.0f - (static_cast<float>(i) / radius);
            fillCircle(centerX, centerY, static_cast<float>(i), mixColor(inner, outer, t));
        }
    }

    void Renderer::drawGlowCircle(float centerX, float centerY, float radius, SDL_Color color, int layers)
    {
        for (int i = layers; i >= 1; --i)
        {
            SDL_Color glow = color;
            glow.a = static_cast<Uint8>(std::max(4, color.a / (i * 2)));
            fillCircle(centerX, centerY, radius + static_cast<float>(i) * 2.3f, glow);
        }
    }

    void Renderer::drawSoftShadow(float x, float y, float w, float h, float radius, int layers, SDL_Color color)
    {
        for (int i = layers; i >= 1; --i)
        {
            SDL_Color shade = color;
            shade.a = static_cast<Uint8>(std::max(2, color.a / (i * 2)));
            fillRoundedRect(x - static_cast<float>(i), y - static_cast<float>(i) + 1.0f, w + static_cast<float>(i) * 2.0f,
                            h + static_cast<float>(i) * 2.0f, radius + static_cast<float>(i), shade);
        }
    }

    void Renderer::drawProgressBar(float x, float y, float w, float h, float progress, SDL_Color track, SDL_Color fill)
    {
        const float clamped = std::clamp(progress, 0.0f, 1.0f);
        fillRoundedRect(x, y, w, h, h * 0.5f, track);
        fillRoundedRect(x, y, std::max(h, w * clamped), h, h * 0.5f, fill);
    }

    void Renderer::setClipRect(float x, float y, float w, float h)
    {
        const SDL_Rect clip {
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(w),
            static_cast<int>(h)
        };
        SDL_SetRenderClipRect(m_renderer, &clip);
    }

    void Renderer::clearClipRect()
    {
        SDL_SetRenderClipRect(m_renderer, nullptr);
    }

    void Renderer::drawText(const std::string& text, float x, float y, SDL_Color color, int size)
    {
        const float textScale = std::max(1.0f, static_cast<float>(size) / 8.0f);
        SDL_SetRenderScale(m_renderer, textScale, textScale);
        setDrawColor(color);
        SDL_RenderDebugText(m_renderer, x / textScale, y / textScale, text.c_str());
        SDL_SetRenderScale(m_renderer, 1.0f, 1.0f);
    }

    void Renderer::drawTextCentered(const std::string& text, float centerX, float y, SDL_Color color, int size)
    {
        const float glyphWidth = static_cast<float>(size) * 0.6f;
        const float width = glyphWidth * static_cast<float>(text.size());
        drawText(text, centerX - width * 0.5f, y, color, size);
    }
}
