#include "Display.h"

bool Display::begin()
{
    Serial.println("[Display] Initializing...");

    // TODO:
    // Initialize LovyanGFX
    // Initialize LVGL
    // Configure display
    // Configure touch

    Serial.println("[Display] Ready");

    return true;
}

void Display::update()
{
    // LVGL task handler will go here
}

void Display::clear()
{
    // Screen clear implementation
}

uint16_t Display::width()
{
    return 320;
}

uint16_t Display::height()
{
    return 240;
}