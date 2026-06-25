#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace VOXA
{

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool initialize(
            const char *title,
            int width,
            int height);

        void shutdown();

        bool isRunning() const;

        void processEvents();

        void beginFrame();

        void endFrame();

        //---------------------------------------------------------
        // Drawing
        //---------------------------------------------------------

        void clear(SDL_Color color);

        void drawPixel(
            int x,
            int y,
            SDL_Color color);

        void drawLine(
            int x1,
            int y1,
            int x2,
            int y2,
            SDL_Color color);

        void drawRect(
            int x,
            int y,
            int w,
            int h,
            SDL_Color color);

        void fillRect(
            int x,
            int y,
            int w,
            int h,
            SDL_Color color);

        void drawCircle(
            int centerX,
            int centerY,
            int radius,
            SDL_Color color);

        void fillCircle(
            int centerX,
            int centerY,
            int radius,
            SDL_Color color);

        //---------------------------------------------------------
        // Premium Widgets
        //---------------------------------------------------------

        void drawRoundedRect(
            int x,
            int y,
            int w,
            int h,
            int radius,
            SDL_Color color);

        void fillRoundedRect(
            int x,
            int y,
            int w,
            int h,
            int radius,
            SDL_Color color);

        void drawShadow(
            int x,
            int y,
            int w,
            int h,
            int radius,
            int blur);

        void drawProgressBar(
            int x,
            int y,
            int w,
            int h,
            float progress,
            SDL_Color background,
            SDL_Color foreground);

        //---------------------------------------------------------
        // Future
        //---------------------------------------------------------

        void drawText(
            const std::string &text,
            int x,
            int y,
            SDL_Color color,
            int size);

        void drawImage(
            const std::string &path,
            int x,
            int y,
            int w,
            int h);

        //---------------------------------------------------------

        SDL_Renderer *getSDLRenderer();

    private:
        SDL_Window *window;
        SDL_Renderer *renderer;

        bool running;

        int width;
        int height;
    };

}