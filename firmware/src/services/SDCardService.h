#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <cstdint>

#include "../storage/StorageManager.h"

namespace VOXA
{
    /**
     * @brief Modular MicroSD Card Service for VOXA firmware (integrated with StorageManager).
     */
    class SDCardService
    {
    public:
        static constexpr gpio_num_t SD_CS_PIN   = StorageManager::SD_CS_PIN;
        static constexpr gpio_num_t SD_MOSI_PIN = StorageManager::SD_MOSI_PIN;
        static constexpr gpio_num_t SD_MISO_PIN = StorageManager::SD_MISO_PIN;
        static constexpr gpio_num_t SD_SCK_PIN  = StorageManager::SD_SCK_PIN;

        SDCardService() = default;
        ~SDCardService() = default;

        bool begin() { return storageManager.begin(); }
        void refreshStorageInfo() {}
        void logPush(const char* action, const char* path, size_t sizeBytes, const char* itemType);
        bool checkPresence() { return storageManager.isCardAttached(); }

        [[nodiscard]] bool isCardAttached() const { return storageManager.isCardAttached(); }
        [[nodiscard]] bool isMounted() const { return storageManager.isSdMounted(); }
        [[nodiscard]] uint64_t getCardSizeMB() const { return storageManager.getTotalSpaceMB(); }
        [[nodiscard]] uint64_t getTotalSpaceMB() const { return storageManager.getTotalSpaceMB(); }
        [[nodiscard]] uint64_t getUsedSpaceMB() const { return storageManager.getUsedSpaceMB(); }
        [[nodiscard]] uint64_t getFreeSpaceMB() const { return storageManager.getFreeSpaceMB(); }
    };

    extern SDCardService sdCardService;
}
