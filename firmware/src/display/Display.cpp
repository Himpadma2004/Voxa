#include "Display.h"

LGFX Display::lcd;

bool Display::begin()
{
    Serial.begin(115200);

    lcd.init();

    lcd.setRotation(1);

    lcd.fillScreen(TFT_BLACK);

    Serial.println("[Display] Ready");

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