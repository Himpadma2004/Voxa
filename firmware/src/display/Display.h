#ifndef VOXA_DISPLAY_H
#define VOXA_DISPLAY_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../../include/LGFX_Config.hpp"

class Display
{
public:
    static LGFX lcd;

    static bool begin();

    static void update();

    static void clear();

    static uint16_t width();

    static uint16_t height();

    static uint8_t getRotation();

    static void setRotation(uint8_t rotation);

    static void setBrightness(uint8_t brightness);

    static uint8_t getBrightness();
};

#endif