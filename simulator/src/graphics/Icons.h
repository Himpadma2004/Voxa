#pragma once

#include <SDL3/SDL.h>

namespace VOXA
{
    class Renderer;

    enum class Icon
    {
        Mic,
        Bell,
        Lightbulb,
        Question,
        Search,
        Folder,
        Settings,
        ChevronRight,
        Back,
        Plus,
        Filter,
        Wifi,
        Battery,
        Calendar,
        Cloud,
        Storage,
        Info,
        Star,
        Note,
        Chat,
        Spark,
        Upload
    };

    void drawIcon(Renderer& renderer, Icon icon, float x, float y, float size, SDL_Color color);
}
