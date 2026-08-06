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
        Detail,
        RecordingsLibrary,
        AudioPlayer,
        WiFiSettings,
        TextInput,
        Tasks
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
        WiFiOff,
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
        Rotate,
        Play,
        Pause,
        Power,
        Reset
    };


    namespace ScreenCommon
    {
        // Shared Screen Layout Drawings
        void renderSurface(LovyanGFX& canvas, uint16_t w, uint16_t h);
        
        void renderPageDots(LovyanGFX& canvas, int activeIndex, int count, uint16_t w, uint16_t h);
        
        void renderCircularButton(LovyanGFX& canvas, float centerX, float centerY, Icon icon, 
                                  uint16_t fill, uint16_t iconColor, uint16_t w, uint16_t h);
                                  
        void renderHeader(LovyanGFX& canvas, const std::string& title, bool showBack, 
                          bool showRightAction, Icon rightIcon, uint16_t w, uint16_t h);

        // Pixel-perfect Geometric Icon Drawing (replaces desktop fonts)
        void drawIcon(LovyanGFX& canvas, Icon icon, float x, float y, float size, uint16_t color);
        
        // Microphone geometric shape
        void drawMicShape(LovyanGFX& canvas, float cx, float cy, float size, uint16_t color, uint16_t bgColor);
    }
}

#endif // VOXA_SCREENCOMMON_H
