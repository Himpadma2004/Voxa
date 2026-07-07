#ifndef VOXA_BOOTSCREEN_H
#define VOXA_BOOTSCREEN_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../display/Display.h"

class BootScreen
{
public:
    BootScreen();

    void show();

private:
    void drawBackground(LGFX_Sprite& canvas, uint16_t w, uint16_t h);
    
    void drawGlowCircle(LGFX_Sprite& canvas, float cx, float cy, float radius, 
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a, 
                        int layers, uint16_t h);
                        
    void drawWaves(LGFX_Sprite& canvas, float elapsed, uint16_t w, uint16_t h);
    
    void drawProgressBar(LGFX_Sprite& canvas, float progress, uint16_t w, uint16_t h);
};

#endif // VOXA_BOOTSCREEN_H