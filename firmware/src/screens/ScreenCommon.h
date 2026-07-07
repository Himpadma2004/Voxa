#ifndef VOXA_SCREENCOMMON_H
#define VOXA_SCREENCOMMON_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <string>

namespace VOXA
{
    enum class ScreenId
    {
        Boot,
        Home,
        Record,
        Search,
        Reminders,
        Questions,
        Ideas,
        Others,
        Settings,
        SyncStatus,
        Detail
    };

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
        Upload,
        Rotate
    };

    namespace ScreenCommon
    {
        // Shared Screen Layout Drawings
        void renderSurface(LGFX_Sprite& canvas, uint16_t w, uint16_t h);
        
        void renderPageDots(LGFX_Sprite& canvas, int activeIndex, int count, uint16_t w, uint16_t h);
        
        void renderCircularButton(LGFX_Sprite& canvas, float centerX, float centerY, Icon icon, 
                                  uint16_t fill, uint16_t iconColor, uint16_t w, uint16_t h);
                                  
        void renderHeader(LGFX_Sprite& canvas, const std::string& title, bool showBack, 
                          bool showRightAction, Icon rightIcon, uint16_t w, uint16_t h);

        // Pixel-perfect Geometric Icon Drawing (replaces desktop fonts)
        void drawIcon(LGFX_Sprite& canvas, Icon icon, float x, float y, float size, uint16_t color);
        
        // Microphone geometric shape
        void drawMicShape(LGFX_Sprite& canvas, float cx, float cy, float size, uint16_t color, uint16_t bgColor);
    }
}

#endif // VOXA_SCREENCOMMON_H
