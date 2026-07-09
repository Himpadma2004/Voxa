#include "Touch.h"

bool Touch::begin()
{
    Serial.println("[Touch] Initializing...");

    Wire.begin(8, 18);

    if (!touch.begin())
    {
        Serial.println("[Touch] FAILED");
        return false;
    }

    touch.setRotation(1);

    Serial.println("[Touch] Ready");

    return true;
}

bool Touch::isTouched()
{
    return touch.getTouches() > 0;
}

bool Touch::getPoint(uint16_t &x, uint16_t &y)
{
    bool touched = isTouched();
    if (!touched)
    {
        if (m_wasTouched)
        {
            m_wasTouched = false;
            Serial.println("[Touch] Release");
        }
        return false;
    }

    auto p = touch.getPoint(0);
    x = p.x;
    y = p.y;

    if (!m_wasTouched)
    {
        m_wasTouched = true;
        Serial.printf("[Touch] Press: x=%d, y=%d\n", x, y);
    }

    return true;
}

void Touch::setRotation(uint8_t rotation)
{
    touch.setRotation(rotation);
}