#ifndef VOXA_TOUCH_H
#define VOXA_TOUCH_H

#include <Arduino.h>
#include <Wire.h>
#include <CSE_CST328.h>

class Touch
{
public:
    bool begin();

    bool isTouched();

    bool getPoint(uint16_t &x, uint16_t &y);

    void setRotation(uint8_t rotation);

private:
    bool m_wasTouched = false;
    uint32_t m_lastPollMs { 0 };
    CSE_CST328 touch =
        CSE_CST328(
            240,
            320,
            &Wire,
            17, // TP_RST (CST328 hardware reset pin)
            16  // TP_INT (CST328 hardware interrupt pin)
        );
};

#endif