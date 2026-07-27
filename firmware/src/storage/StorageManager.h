#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SdFat.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <cstdint>

namespace VOXA
{
    /**
     * @brief Comprehensive Storage Diagnostics Report structure.
     */
    struct StorageDiagnostics
    {
        std::string activeFilesystem; // "MicroSD" or "SPIFFS"
        std::string storageMode;      // "Primary" or "Fallback"
        std::string cardType;         // SDSC, SDHC, SDXC, None
        std::string filesystem;       // FAT16, FAT32, exFAT, SPIFFS
        uint64_t capacityMB{0};
        uint64_t usedSpaceMB{0};
        uint64_t freeSpaceMB{0};
        uint32_t clusterSizeBytes{4096};
        uint32_t blockSizeBytes{512};
        float readSpeedKBs{0.0f};
        float writeSpeedKBs{0.0f};
        uint32_t mountTimeMs{0};
        bool readTestPassed{false};
        bool writeTestPassed{false};
        bool deleteTestPassed{false};
        bool renameTestPassed{false};
    };

    /**
     * @brief Production Storage Manager for VOXA hardware & firmware.
     * Centralized filesystem access, user/system data separation,
     * filesystem-aware logging, accurate benchmarks, and status summaries.
     */
    class StorageManager
    {
    public:
        // Hardware SPI Pins for SD Card
        static constexpr gpio_num_t SD_CS_PIN = GPIO_NUM_1;
        static constexpr gpio_num_t SD_MOSI_PIN = GPIO_NUM_2;
        static constexpr gpio_num_t SD_MISO_PIN = GPIO_NUM_13;
        static constexpr gpio_num_t SD_SCK_PIN = GPIO_NUM_21;

        StorageManager();
        ~StorageManager();

        /**
         * @brief Mounts storage, initializes directories, runs health checks, prints summary.
         * @return true if mounted successfully (SD or SPIFFS).
         */
        bool begin();

        /**
         * @brief Runs complete storage diagnostics & speed benchmarks for active media.
         */
        StorageDiagnostics runDiagnostics();

        /**
         * @brief Automatically creates standard VOXA directory hierarchy on first boot.
         */
        void initializeDirectories();

        /**
         * @brief Prints concise VOXA Storage Status Summary table.
         */
        void printStorageSummary();

        /**
         * @brief Centralized Storage API: Saves audio recording to /recordings
         */
        bool saveRecording(const char *filename, const uint8_t *data, size_t len);

        /**
         * @brief Centralized Storage API: Saves Whisper transcript to /transcripts
         */
        bool saveTranscript(const char *filename, const char *text);

        /**
         * @brief Centralized Storage API: Saves AI summary to /summaries
         */
        bool saveSummary(const char *filename, const char *text);

        /**
         * @brief Centralized Storage API: Appends log message to /logs/device.log
         */
        bool saveLog(const char *message);

        /**
         * @brief Centralized JSON Storage API: Saves JSON document to specified path
         */
        bool saveJson(const char *path, const JsonDocument &doc);

        /**
         * @brief Centralized JSON Storage API: Loads JSON document from specified path
         */
        bool loadJson(const char *path, JsonDocument &doc);

        /**
         * @brief Centralized File Operations API: Deletes file at path
         */
        bool deleteFile(const char *path);

        /**
         * @brief Centralized File Operations API: Renames file
         */
        bool renameFile(const char *oldPath, const char *newPath);

        /**
         * @brief Centralized Directory Listing API: Lists files in dirPath
         */
        std::vector<std::string> listDirectory(const char *dirPath);

        /**
         * @brief Centralized Automatic Cleanup API: Deletes temporary files, old cache, and failed uploads
         */
        void performAutomaticCleanup();

        /**
         * @brief Returns human-readable storage summary string
         */
        std::string getStorageInfoString();

        /**
         * @brief Centralized Filesystem Resolver: Resolves active FS for target path.
         * Enforces System Data (Config/Settings) on SPIFFS, User Data on MicroSD/SPIFFS.
         */
        FS &getFSForPath(const char *path = nullptr);

        /**
         * @brief Returns active filesystem name string for target path.
         */
        const char *getFSNameForPath(const char *path = nullptr);

        [[nodiscard]] bool isSdMounted() const { return m_sdMounted; }
        [[nodiscard]] bool isSpiffsMounted() const { return m_spiffsMounted; }
        [[nodiscard]] bool isCardAttached() const { return m_cardAttached; }

        [[nodiscard]] uint64_t getTotalSpaceMB() const;
        [[nodiscard]] uint64_t getUsedSpaceMB() const;
        [[nodiscard]] uint64_t getFreeSpaceMB() const;

    private:
        bool m_sdMounted{false};
        bool m_spiffsMounted{false};
        bool m_cardAttached{false};
        uint32_t m_mountTimeMs{0};
        std::string m_lastErrorCode{"SD_ERR_NONE"};

        bool mountSdCard();
        std::string resolvePath(const char *category, const char *filename);
    };

    extern StorageManager storageManager;
}