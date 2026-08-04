#include "Display.h"

LGFX Display::lcd;

static uint8_t s_brightness = 130; // ~50% energy-efficient default (prevents HW122 voltage sag / brownout)

bool Display::begin()
{
    Serial.begin(115200);

    lcd.init();
    setBrightness(130); // ~50% brightness to lower power demand on VCC rail
    lcd.setRotation(1);
    lcd.fillScreen(TFT_BLACK);

    Serial.println("[Display] Ready (Power-Efficient Backlight Active)");

    return true;
}

void Display::update()
{
}

void Display::clear()
{
    lcd.fillScreen(TFT_BLACK);
}

uint16_t Display::width()
{
    return lcd.width();
}

uint16_t Display::height()
{
    return lcd.height();
}

uint8_t Display::getRotation()
{
    return lcd.getRotation();
}

void Display::setRotation(uint8_t rotation)
{
    lcd.setRotation(rotation);
}

void Display::setBrightness(uint8_t brightness)
{
    s_brightness = brightness;
    lcd.setBrightness(brightness);
    Serial.printf("[Display] Backlight brightness set to %u / 255 (%u%%)\n", brightness, (uint16_t)brightness * 100 / 255);
}

uint8_t Display::getBrightness()
{
    return s_brightness;
}