#include "SDCardService.h"

namespace VOXA
{
    SDCardService sdCardService;

    void SDCardService::logPush(const char* action, const char* path, size_t sizeBytes, const char* itemType)
    {
        Serial.println("================================================================================");
        Serial.printf("[SD CARD PUSH] Action: %s | File: %s | Size: %u bytes | Type: %s\n",
                      action, path, (unsigned int)sizeBytes, itemType);
        Serial.printf("[SD CARD STORAGE] %s\n", storageManager.getStorageInfoString().c_str());
        Serial.println("================================================================================");
    }
}