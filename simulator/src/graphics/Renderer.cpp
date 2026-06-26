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

    std::string codepointToUtf8(char32_t codepoint)
    {
        std::string utf8;
        if (codepoint < 0x80)
        {
            utf8 += static_cast<char>(codepoint);
        }
        else if (codepoint < 0x800)
        {
            utf8 += static_cast<char>((codepoint >> 6) | 0xC0);
            utf8 += static_cast<char>((codepoint & 0x3F) | 0x80);
        }
        else if (codepoint < 0x10000)
        {
            utf8 += static_cast<char>((codepoint >> 12) | 0xE0);
            utf8 += static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
            utf8 += static_cast<char>((codepoint & 0x3F) | 0x80);
        }
        else
        {
            utf8 += static_cast<char>((codepoint >> 18) | 0xF0);
            utf8 += static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80);
            utf8 += static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
            utf8 += static_cast<char>((codepoint & 0x3F) | 0x80);
        }
        return utf8;
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

        if (!TTF_Init())
        {
            SDL_Quit();
            return false;
        }

        m_window = SDL_CreateWindow(title, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (m_window == nullptr)
        {
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        m_renderer = SDL_CreateRenderer(m_window, nullptr);
        if (m_renderer == nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            TTF_Quit();
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

        for (auto& pair : m_fonts)
        {
            if (pair.second != nullptr)
            {
                TTF_CloseFont(pair.second);
            }
        }
        m_fonts.clear();

        for (auto& pair : m_iconFonts)
        {
            if (pair.second != nullptr)
            {
                TTF_CloseFont(pair.second);
            }
        }
        m_iconFonts.clear();

        TTF_Quit();

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
        int winW = 0, winH = 0;
        SDL_GetRenderOutputSize(m_renderer, &winW, &winH);
        
        float scaleX = static_cast<float>(winW) / static_cast<float>(m_canvasWidth);
        float scaleY = static_cast<float>(winH) / static_cast<float>(m_canvasHeight);
        m_letterboxScale = std::min(scaleX, scaleY);
        
        m_letterboxViewport.w = static_cast<int>(static_cast<float>(m_canvasWidth) * m_letterboxScale);
        m_letterboxViewport.h = static_cast<int>(static_cast<float>(m_canvasHeight) * m_letterboxScale);
        m_letterboxViewport.x = (winW - m_letterboxViewport.w) / 2;
        m_letterboxViewport.y = (winH - m_letterboxViewport.h) / 2;
        
        clear(SDL_Color{ 0, 0, 0, 255 });
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
        if (radius <= 0.0f)
        {
            drawRect(x, y, w, h, color);
            return;
        }

        // Draw straight lines
        drawLine(x + radius, y, x + w - radius, y, color);
        drawLine(x + radius, y + h, x + w - radius, y + h, color);
        drawLine(x, y + radius, x, y + h - radius, color);
        drawLine(x + w, y + radius, x + w, y + h - radius, color);

        // Draw 4 corner arcs
        const int segments = 16;
        for (int i = 0; i < segments; ++i)
        {
            const float a0 = static_cast<float>(i) / segments * (kPi * 0.5f);
            const float a1 = static_cast<float>(i + 1) / segments * (kPi * 0.5f);

            // Top-Right: 0 to Pi/2
            drawLine(x + w - radius + std::cos(a0) * radius, y + radius - std::sin(a0) * radius,
                     x + w - radius + std::cos(a1) * radius, y + radius - std::sin(a1) * radius, color);

            // Top-Left: Pi/2 to Pi
            drawLine(x + radius - std::sin(a0) * radius, y + radius - std::cos(a0) * radius,
                     x + radius - std::sin(a1) * radius, y + radius - std::cos(a1) * radius, color);

            // Bottom-Left: Pi to 3Pi/2
            drawLine(x + radius - std::cos(a0) * radius, y + h - radius + std::sin(a0) * radius,
                     x + radius - std::cos(a1) * radius, y + h - radius + std::sin(a1) * radius, color);

            // Bottom-Right: 3Pi/2 to 2Pi
            drawLine(x + w - radius + std::sin(a0) * radius, y + h - radius + std::cos(a0) * radius,
                     x + w - radius + std::sin(a1) * radius, y + h - radius + std::cos(a1) * radius, color);
        }
    }

    void Renderer::fillRoundedRect(float x, float y, float w, float h, float radius, SDL_Color color)
    {
        if (radius <= 0.0f)
        {
            fillRect(x, y, w, h, color);
            return;
        }

        setDrawColor(color);
        const int r = static_cast<int>(radius);
        const int ix = static_cast<int>(x);
        const int iy = static_cast<int>(y);
        const int iw = static_cast<int>(w);
        const int ih = static_cast<int>(h);

        for (int row = 0; row < ih; ++row)
        {
            float dx = 0.0f;
            if (row < r)
            {
                const float dy = static_cast<float>(r - row);
                dx = radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
            }
            else if (row >= ih - r)
            {
                const float dy = static_cast<float>(row - (ih - r - 1));
                dx = radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy));
            }

            const float startX = static_cast<float>(ix) + dx;
            const float endX = static_cast<float>(ix + iw) - dx;
            SDL_RenderLine(m_renderer, startX, static_cast<float>(iy + row), endX, static_cast<float>(iy + row));
        }
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
        const int steps = 16;
        for (int i = steps; i >= 1; --i)
        {
            const float r = radius * (static_cast<float>(i) / steps);
            const float t = 1.0f - (static_cast<float>(i) / steps);
            fillCircle(centerX, centerY, r, mixColor(inner, outer, t));
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

    void Renderer::drawGlassCard(float x, float y, float w, float h, float radius, SDL_Color shadowColor, SDL_Color fillColor, SDL_Color borderColor)
    {
        drawSoftShadow(x, y, w, h, radius, 8, shadowColor);
        fillRoundedRect(x, y, w, h, radius, fillColor);
        if (borderColor.a > 0)
        {
            drawRoundedRect(x, y, w, h, radius, borderColor);
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

    TTF_Font* Renderer::getFont(int size) const
    {
        auto it = m_fonts.find(size);
        if (it != m_fonts.end())
        {
            return it->second;
        }

        // Try Loading Segoe UI (default Windows system font)
        TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/segoeui.ttf", static_cast<float>(size));
        if (font == nullptr)
        {
            // Fallback 1: Arial
            font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", static_cast<float>(size));
        }

        m_fonts[size] = font;
        return font;
    }

    void Renderer::drawText(const std::string& text, float x, float y, SDL_Color color, int size)
    {
        if (text.empty()) return;
        TTF_Font* font = getFont(size);
        if (font != nullptr)
        {
            SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), 0, color);
            if (surface != nullptr)
            {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
                if (texture != nullptr)
                {
                    const float w = static_cast<float>(surface->w);
                    const float h = static_cast<float>(surface->h);
                    const SDL_FRect dstRect { x, y, w, h };
                    SDL_RenderTexture(m_renderer, texture, nullptr, &dstRect);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(surface);
                return;
            }
        }

        // Fallback to debug text
        const float textScale = std::max(1.0f, static_cast<float>(size) / 8.0f);
        SDL_SetRenderScale(m_renderer, textScale, textScale);
        setDrawColor(color);
        SDL_RenderDebugText(m_renderer, x / textScale, y / textScale, text.c_str());
        SDL_SetRenderScale(m_renderer, 1.0f, 1.0f);
    }

    void Renderer::drawTextCentered(const std::string& text, float centerX, float y, SDL_Color color, int size)
    {
        if (text.empty()) return;
        TTF_Font* font = getFont(size);
        if (font != nullptr)
        {
            SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), 0, color);
            if (surface != nullptr)
            {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
                if (texture != nullptr)
                {
                    const float w = static_cast<float>(surface->w);
                    const float h = static_cast<float>(surface->h);
                    const SDL_FRect dstRect { centerX - w * 0.5f, y, w, h };
                    SDL_RenderTexture(m_renderer, texture, nullptr, &dstRect);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(surface);
                return;
            }
        }

        // Fallback to debug text centered
        const float glyphWidth = static_cast<float>(size) * 0.6f;
        const float width = glyphWidth * static_cast<float>(text.size());
        drawText(text, centerX - width * 0.5f, y, color, size);
    }

    TTF_Font* Renderer::getIconFont(int size) const
    {
        auto it = m_iconFonts.find(size);
        if (it != m_iconFonts.end())
        {
            return it->second;
        }

        // Try Loading Segoe MDL2 Assets (default Windows built-in icon font)
        TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/segmdl2.ttf", static_cast<float>(size));
        if (font == nullptr)
        {
            // Fallback to Segoe Fluent Icons
            font = TTF_OpenFont("C:/Windows/Fonts/SegoeFluentIcons.ttf", static_cast<float>(size));
        }
        if (font == nullptr)
        {
            // Fallback to Webdings
            font = TTF_OpenFont("C:/Windows/Fonts/webdings.ttf", static_cast<float>(size));
        }
        if (font == nullptr)
        {
            // Fallback to Arial
            font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", static_cast<float>(size));
        }

        m_iconFonts[size] = font;
        return font;
    }

    void Renderer::drawIconGlyph(char32_t codepoint, float cx, float cy, SDL_Color color, int size)
    {
        TTF_Font* font = getIconFont(size);
        if (font != nullptr)
        {
            std::string text = codepointToUtf8(codepoint);
            SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), 0, color);
            if (surface != nullptr)
            {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
                if (texture != nullptr)
                {
                    const float w = static_cast<float>(surface->w);
                    const float h = static_cast<float>(surface->h);
                    // Center the glyph exactly at (cx, cy)
                    const SDL_FRect dstRect { cx - w * 0.5f, cy - h * 0.5f, w, h };
                    SDL_RenderTexture(m_renderer, texture, nullptr, &dstRect);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(surface);
                return;
            }
        }

        // Fallback: draw a simple small dot or circle
        drawCircle(cx, cy, static_cast<float>(size) * 0.35f, color);
    }

    void Renderer::setLogicalOffset(float offsetX, float offsetY)
    {
        SDL_Rect vp;
        vp.x = m_letterboxViewport.x + static_cast<int>(offsetX * m_letterboxScale);
        vp.y = m_letterboxViewport.y + static_cast<int>(offsetY * m_letterboxScale);
        vp.w = m_letterboxViewport.w;
        vp.h = m_letterboxViewport.h;
        SDL_SetRenderViewport(m_renderer, &vp);
    }

    void Renderer::resetLogicalOffset()
    {
        SDL_SetRenderViewport(m_renderer, nullptr);
    }
}
