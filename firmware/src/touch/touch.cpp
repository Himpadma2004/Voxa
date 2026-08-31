#include "Touch.h"
#include "../services/PowerManager.h"
#include "../audio/AudioManager.h"

bool Touch::begin()
{
    Serial.println("[Touch] Initializing...");

    Wire.begin(8, 18);
    Wire.setClock(400000);
    Wire.setTimeOut(100);

    if (!touch.begin())
    {
        Serial.println("[Touch] FAILED");
        return false;
    }

    // Re-assert SDA (8) & SCL (18) and clock in case library default Wire.begin() changed pins
    Wire.begin(8, 18);
    Wire.setClock(400000);
    Wire.setTimeOut(100);

    touch.setRotation(1);

    Serial.println("[Touch] Ready");

    return true;
}

bool Touch::isTouched()
{
    uint32_t now = millis();

    // Rate-limit idle polling to once per 10ms (100 Hz) for responsive touch response.
    // When screen is touched (drag/swipe in progress), poll at full speed.
    if (!m_wasTouched && (now - m_lastPollMs < 10))
    {
        return false;
    }

    m_lastPollMs = now;
    return touch.getTouches() > 0;
}



void Touch::setRotation(uint8_t rotation)
{
    touch.setRotation(rotation);
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
        VOXA::PowerManager::instance().reportActivity();
        if (!VOXA::AudioManager::instance().isPlaying() && !VOXA::AudioManager::instance().isBackgroundPlaying())
        {
            VOXA::AudioManager::instance().playTapSoundAsync();
        }
        Serial.printf("[Touch] Press: x=%d, y=%d\n", x, y);
    }
    else
    {
        VOXA::PowerManager::instance().reportActivity();
    }

    return true;
}
