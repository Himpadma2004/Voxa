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
        // Minimal Obsidian Pitch Black (#000000 / OLED deep black)
        return (currentMode == ThemeMode::Dark) ? 0x0000 : 0xF7BE;
    }

    uint16_t getSurface()
    {
        // Deep Charcoal Card Surface (#141418)
        return (currentMode == ThemeMode::Dark) ? 0x10A3 : 0xFFFF;
    }

    uint16_t getPrimary()
    {
        // Electric Vibrant Orange (#FF6600)
        return 0xFD40;
    }

    uint16_t getPrimaryLight()
    {
        // Warm Soft Amber Orange (#FF9933)
        return 0xFDCD;
    }

    uint16_t getAccent()
    {
        // Vivid Neon Orange (#FF5500)
        return 0xFA00;
    }

    uint16_t getSuccess()
    {
        // Smooth Emerald Green (#10B981)
        return 0x13E0;
    }

    uint16_t getWarning()
    {
        // Coral Crimson Red (#EF4444)
        return 0xEA28;
    }

    uint16_t getTextPrimary()
    {
        // Crisp Pure White (#FFFFFF)
        return (currentMode == ThemeMode::Dark) ? 0xFFFF : 0x0000;
    }

    uint16_t getTextSecondary()
    {
        // Soft Muted Warm Grey (#A0A0A8)
        return (currentMode == ThemeMode::Dark) ? 0x9E15 : 0x630C;
    }

    uint16_t getDivider()
    {
        // Subtle Dark Charcoal Border (#222228)
        return (currentMode == ThemeMode::Dark) ? 0x2125 : 0xE73C;
    }
}