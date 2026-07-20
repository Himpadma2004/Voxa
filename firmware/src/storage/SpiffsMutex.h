#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace VOXA
{
    extern SemaphoreHandle_t g_spiffsMutex;

    void initSpiffsMutex();

    class SpiffsLock
    {
    public:
        explicit SpiffsLock(const char* tag) : m_tag(tag)
        {
            if (g_spiffsMutex != nullptr)
            {
                xSemaphoreTake(g_spiffsMutex, portMAX_DELAY);
                Serial.printf("[SPIFFS] LOCK ACQUIRED by %s\n", m_tag);
            }
        }

        ~SpiffsLock()
        {
            if (g_spiffsMutex != nullptr)
            {
                Serial.printf("[SPIFFS] LOCK RELEASED by %s\n", m_tag);
                xSemaphoreGive(g_spiffsMutex);
            }
        }

        SpiffsLock(const SpiffsLock&) = delete;
        SpiffsLock& operator=(const SpiffsLock&) = delete;

    private:
        const char* m_tag;
    };
}
