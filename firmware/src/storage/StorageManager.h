#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include <cstdint>

// NOTE: SdFat / SD / SPI includes removed — SD card adapter unplugged.
// StorageManager now manages SPIFFS only (used as a temporary WAV buffer
// during the record → cloud-upload window).

namespace VOXA
{
    /**
     * @brief Lightweight diagnostics for the active (SPIFFS-only) filesystem.
     */
    struct StorageDiagnostics
    {
        std::string activeFilesystem { "SPIFFS" };
        std::string storageMode      { "Cloud-Primary" };
        std::string filesystem       { "SPIFFS" };
        uint64_t    capacityMB       { 0 };
        uint64_t    usedSpaceMB      { 0 };
        uint64_t    freeSpaceMB      { 0 };
        bool        readTestPassed   { false };
        bool        writeTestPassed  { false };
        bool        deleteTestPassed { false };
        bool        renameTestPassed { false };
    };

    /**
     * @brief Minimal StorageManager — SPIFFS only.
     * All persistent data is stored in the cloud (MongoDB + AWS S3).
     * SPIFFS is used solely as a temporary staging area for WAV recordings
     * between capture and HTTP upload.
     */
    class StorageManager
    {
    public:
        StorageManager();
        ~StorageManager();

        /**
         * @brief Mounts SPIFFS. SD card is no longer present.
         * @return true if SPIFFS mounted successfully.
         */
        bool begin();

        /**
         * @brief Runs basic SPIFFS health diagnostics.
         */
        StorageDiagnostics runDiagnostics();

        /**
         * @brief Creates standard VOXA temp directory on SPIFFS.
         */
        void initializeDirectories();

        /**
         * @brief Prints concise storage status to Serial.
         */
        void printStorageSummary();

        /**
         * @brief Saves audio recording temp file to SPIFFS /recordings
         */
        bool saveRecording(const char* filename, const uint8_t* data, size_t len);

        /**
         * @brief Saves Whisper transcript to SPIFFS /transcripts
         */
        bool saveTranscript(const char* filename, const char* text);

        /**
         * @brief Saves AI summary to SPIFFS /summaries
         */
        bool saveSummary(const char* filename, const char* text);

        /**
         * @brief Appends log message to SPIFFS /logs/device.log
         */
        bool saveLog(const char* message);

        /**
         * @brief Saves JSON document to SPIFFS path.
         */
        bool saveJson(const char* path, const JsonDocument& doc);

        /**
         * @brief Loads JSON document from SPIFFS path.
         */
        bool loadJson(const char* path, JsonDocument& doc);

        bool deleteFile(const char* path);
        bool renameFile(const char* oldPath, const char* newPath);
        std::vector<std::string> listDirectory(const char* dirPath);

        /**
         * @brief Deletes temporary/old SPIFFS files to free space before recording.
         */
        void performAutomaticCleanup();

        std::string getStorageInfoString();

        FS& getFSForPath(const char* path = nullptr);
        const char* getFSNameForPath(const char* path = nullptr);

        [[nodiscard]] bool     isSdMounted()     const { return false; }
        [[nodiscard]] bool     isSpiffsMounted()  const { return m_spiffsMounted; }
        [[nodiscard]] bool     isCardAttached()   const { return false; }

        [[nodiscard]] uint64_t getTotalSpaceMB()  const;
        [[nodiscard]] uint64_t getUsedSpaceMB()   const;
        [[nodiscard]] uint64_t getFreeSpaceMB()   const;

    private:
        bool        m_spiffsMounted  { false };
        uint32_t    m_mountTimeMs    { 0 };

        std::string resolvePath(const char* category, const char* filename);
    };

    extern StorageManager storageManager;
}