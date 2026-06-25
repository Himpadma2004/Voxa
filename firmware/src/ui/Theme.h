#ifndef VOXA_THEME_H
#define VOXA_THEME_H

#include <Arduino.h>

namespace VoxaTheme
{
    // ----------------------------
    // Display
    // ----------------------------

    constexpr uint16_t SCREEN_WIDTH = 320;
    constexpr uint16_t SCREEN_HEIGHT = 240;

    // ----------------------------
    // Colors (RGB565)
    // ----------------------------

    constexpr uint16_t BACKGROUND = 0x1082;    // #121212
    constexpr uint16_t SURFACE = 0x18E3;       // #1A1B21
    constexpr uint16_t PRIMARY = 0x79CF;       // Purple
    constexpr uint16_t PRIMARY_LIGHT = 0xA27A; // Light Purple
    constexpr uint16_t ACCENT = 0x067F;        // Cyan
    constexpr uint16_t SUCCESS = 0x266C;       // Green
    constexpr uint16_t WARNING = 0xFD20;       // Amber

    constexpr uint16_t TEXT_PRIMARY = 0xFFFF;   // White
    constexpr uint16_t TEXT_SECONDARY = 0xAD55; // Gray
    constexpr uint16_t DIVIDER = 0x2945;

    // ----------------------------
    // Radius
    // ----------------------------

    constexpr uint8_t CARD_RADIUS = 18;
    constexpr uint8_t BUTTON_RADIUS = 16;

    // ----------------------------
    // Padding
    // ----------------------------

    constexpr uint8_t PADDING_SMALL = 8;
    constexpr uint8_t PADDING_NORMAL = 16;
    constexpr uint8_t PADDING_LARGE = 24;

    // ----------------------------
    // Animation
    // ----------------------------

    constexpr uint16_t FAST_ANIMATION = 120;
    constexpr uint16_t NORMAL_ANIMATION = 250;
    constexpr uint16_t SLOW_ANIMATION = 450;

}

#endif