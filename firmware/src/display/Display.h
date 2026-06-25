#ifndef VOXA_DISPLAY_H
#define VOXA_DISPLAY_H

#include <Arduino.h>

class Display
{
public:
    static bool begin();

    static void update();

    static void clear();

    static uint16_t width();

    static uint16_t height();
};

#endif