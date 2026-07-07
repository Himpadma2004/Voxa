#include "Theme.h"

namespace
{
    VoxaTheme::ThemeMode currentMode = VoxaTheme::ThemeMode::Dark;
}

namespace VoxaTheme
{
    ThemeMode getThemeMode()
    {
        return currentMode;
    }

    void setThemeMode(ThemeMode mode)
    {
        currentMode = mode;
    }

    uint16_t getBackground()
    {
        return (currentMode == ThemeMode::Dark) ? 0x1082 : 0xF7BE;
    }

    uint16_t getSurface()
    {
        return (currentMode == ThemeMode::Dark) ? 0x18E3 : 0xFFFF;
    }

    uint16_t getPrimary()
    {
        return 0x79CF; // Match primary branding accent in both modes
    }

    uint16_t getPrimaryLight()
    {
        return 0xA27A;
    }

    uint16_t getAccent()
    {
        return 0x067F;
    }

    uint16_t getSuccess()
    {
        return 0x266C;
    }

    uint16_t getWarning()
    {
        return 0xFD20;
    }

    uint16_t getTextPrimary()
    {
        return (currentMode == ThemeMode::Dark) ? 0xFFFF : 0x1082;
    }

    uint16_t getTextSecondary()
    {
        return (currentMode == ThemeMode::Dark) ? 0xAD55 : 0x7BEF;
    }

    uint16_t getDivider()
    {
        return (currentMode == ThemeMode::Dark) ? 0x2945 : 0xE73C;
    }
}