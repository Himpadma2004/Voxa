#ifndef VOXA_THEME_H
#define VOXA_THEME_H

#include <Arduino.h>

namespace VoxaTheme
{
    enum class ThemeMode
    {
        Dark,
        Light
    };

    // Theme Mode Accessors
    ThemeMode getThemeMode();
    void setThemeMode(ThemeMode mode);

    // Dynamic Color Palette Lookups
    uint16_t getBackground();
    uint16_t getSurface();
    uint16_t getPrimary();
    uint16_t getPrimaryLight();
    uint16_t getAccent();
    uint16_t getSuccess();
    uint16_t getWarning();
    uint16_t getTextPrimary();
    uint16_t getTextSecondary();
    uint16_t getDivider();

    // ----------------------------
    // Shared Layout Metrics (Keep as constexpr compile-time constants)
    // ----------------------------

    constexpr uint8_t CARD_RADIUS = 18;
    constexpr uint8_t BUTTON_RADIUS = 16;

    constexpr uint8_t PADDING_SMALL = 8;
    constexpr uint8_t PADDING_NORMAL = 16;
    constexpr uint8_t PADDING_LARGE = 24;

    constexpr uint16_t FAST_ANIMATION = 120;
    constexpr uint16_t NORMAL_ANIMATION = 250;
    constexpr uint16_t SLOW_ANIMATION = 450;
}

#endif // VOXA_THEME_H