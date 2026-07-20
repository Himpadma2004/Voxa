#include "SpiffsMutex.h"

namespace VOXA
{
    SemaphoreHandle_t g_spiffsMutex = nullptr;

    void initSpiffsMutex()
    {
        if (g_spiffsMutex == nullptr)
        {
            g_spiffsMutex = xSemaphoreCreateMutex();
            Serial.println("[SPIFFS] Global filesystem mutex initialized successfully.");
        }
    }
}
