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

    bool isDarkMode()
    {
        return (currentMode == ThemeMode::Dark);
    }

    uint16_t getDynamicIslandBg()
    {
        // High-contrast Apple capsule (Always obsidian black in both modes)
        return (currentMode == ThemeMode::Dark) ? 0x1082 : 0x0841;
    }

    uint16_t getDynamicIslandFg()
    {
        // Always crisp pure white text/icons inside Dynamic Island
        return 0xFFFF;
    }

    uint16_t getBackground()
    {
        // Dark: Minimal Obsidian Pitch Black (#000000 / OLED deep black)
        // Light: Apple Porcelain Soft Silver (#F5F7FA)
        return (currentMode == ThemeMode::Dark) ? 0x0000 : 0xF7BE;
    }

    uint16_t getSurface()
    {
        // Dark: Deep Charcoal Card Surface (#141418)
        // Light: Pure Crisp White (#FFFFFF)
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
        // Dark: Crisp Pure White (#FFFFFF)
        // Light: Deep Obsidian Charcoal (#1A1D24) — readable & soft
        return (currentMode == ThemeMode::Dark) ? 0xFFFF : 0x18C3;
    }

    uint16_t getTextSecondary()
    {
        // Dark: Soft Muted Warm Grey (#A0A0A8)
        // Light: Apple Refined Slate Grey (#707684)
        return (currentMode == ThemeMode::Dark) ? 0x9E15 : 0x738E;
    }

    uint16_t getDivider()
    {
        // Dark: Subtle Dark Charcoal Border (#222228)
        // Light: Clean Delicate Platinum Border (#E2E5EC)
        return (currentMode == ThemeMode::Dark) ? 0x2125 : 0xDEFB;
    }

    uint16_t getGlassSurface()
    {
        // Dark: Translucent Dark Frosted Glass (#181A24)
        // Light: Pure Crisp White Card Surface (#FFFFFF)
        return (currentMode == ThemeMode::Dark) ? 0x18C5 : 0xFFFF;
    }

    uint16_t getGlassHighlight()
    {
        // Specular Optical Glass Reflection Rim
        return (currentMode == ThemeMode::Dark) ? 0x4A69 : 0xFFFF;
    }

    uint16_t getGlassBorder()
    {
        // Dark: Subtle iOS Translucent Border (#2A2C3C)
        // Light: Delicate Clean Platinum Border (#DCDFE6)
        return (currentMode == ThemeMode::Dark) ? 0x2965 : 0xCE79;
    }

    uint16_t getSystemBlue()
    {
        // Authentic iOS System Blue (#007AFF)
        return 0x04BF;
    }

    uint16_t getSystemGreen()
    {
        // Authentic iOS System Green (#34C759)
        return 0x364B;
    }

    uint16_t getSystemRed()
    {
        // Authentic iOS System Red / Coral (#FF3B30)
        return 0xF9C7;
    }

    uint16_t getSystemIndigo()
    {
        // Authentic iOS System Indigo (#5856D6)
        return 0x59DF;
    }

    uint16_t getSystemPurple()
    {
        // Authentic iOS System Purple / Music Pink (#AF52DE)
        return 0xAF1F;
    }

    uint16_t getSystemAmber()
    {
        // Authentic iOS System Warm Amber / Orange (#FF9500)
        return 0xFDE0;
    }
}