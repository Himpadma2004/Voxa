#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <unordered_map>

namespace VOXA
{
    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool initialize(const char* title, int windowWidth, int windowHeight, int canvasWidth, int canvasHeight);
        void shutdown();

        void beginFrame();
        void endFrame();

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] SDL_Renderer* sdlRenderer() const;
        [[nodiscard]] int canvasWidth() const;
        [[nodiscard]] int canvasHeight() const;
        [[nodiscard]] float scale() const;
        [[nodiscard]] SDL_FPoint windowToCanvas(float windowX, float windowY) const;

        void clear(SDL_Color color);
        void setDrawColor(SDL_Color color);

        void drawLine(float x1, float y1, float x2, float y2, SDL_Color color);
        void drawRect(float x, float y, float w, float h, SDL_Color color);
        void fillRect(float x, float y, float w, float h, SDL_Color color);
        void drawRoundedRect(float x, float y, float w, float h, float radius, SDL_Color color);
        void fillRoundedRect(float x, float y, float w, float h, float radius, SDL_Color color);
        void fillVerticalGradient(float x, float y, float w, float h, SDL_Color top, SDL_Color bottom);
        void fillCircle(float centerX, float centerY, float radius, SDL_Color color);
        void drawCircle(float centerX, float centerY, float radius, SDL_Color color);
        void fillCircleGradient(float centerX, float centerY, float radius, SDL_Color inner, SDL_Color outer);
        void drawGlowCircle(float centerX, float centerY, float radius, SDL_Color color, int layers);
        void drawSoftShadow(float x, float y, float w, float h, float radius, int layers, SDL_Color color);
        void drawGlassCard(float x, float y, float w, float h, float radius, SDL_Color shadowColor, SDL_Color fillColor, SDL_Color borderColor);
        void drawProgressBar(float x, float y, float w, float h, float progress, SDL_Color track, SDL_Color fill);
        void setClipRect(float x, float y, float w, float h);
        void clearClipRect();
        void drawText(const std::string& text, float x, float y, SDL_Color color, int size);
        void drawTextCentered(const std::string& text, float centerX, float y, SDL_Color color, int size);
        void drawIconGlyph(char32_t codepoint, float cx, float cy, SDL_Color color, int size);
        void setLogicalOffset(float offsetX, float offsetY);
        void resetLogicalOffset();

    private:
        TTF_Font* getFont(int size) const;
        TTF_Font* getIconFont(int size) const;

        SDL_Window* m_window { nullptr };
        SDL_Renderer* m_renderer { nullptr };
        bool m_running { false };
        int m_canvasWidth { 0 };
        int m_canvasHeight { 0 };
        mutable std::unordered_map<int, TTF_Font*> m_fonts;
        mutable std::unordered_map<int, TTF_Font*> m_iconFonts;
        SDL_Rect m_letterboxViewport {};
        float m_letterboxScale { 1.0f };
    };
}
