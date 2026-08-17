#include "StorageManager.h"
#include "../storage/SpiffsMutex.h"

// SD card adapter physically removed — all persistent data is stored in the
// cloud (MongoDB + AWS S3). SPIFFS is retained only as a temporary staging
// area for WAV recordings between capture and HTTP upload.

namespace VOXA
{
    StorageManager storageManager;

    StorageManager::StorageManager() {}
    StorageManager::~StorageManager() {}

    // -----------------------------------------------------------------------
    // begin() — SPIFFS-only init
    // -----------------------------------------------------------------------

    bool StorageManager::begin()
    {
        uint32_t startMs    = millis();
        uint32_t heapBefore = ESP.getFreeHeap();

        Serial.println("=================================");
        Serial.println("[StorageManager] Cloud-Primary Mode — SPIFFS temp storage only");
        Serial.println("[StorageManager] SD card adapter removed. All data -> cloud (MongoDB/S3).");
        Serial.printf("[StorageManager] Heap before init: %u bytes\n", heapBefore);

        // Mount SPIFFS — used for temporary WAV buffering during upload window
        initSpiffsMutex();
        if (SPIFFS.begin(true))
        {
            m_spiffsMounted = true;
            Serial.println("[StorageManager] SPIFFS mounted successfully.");
        }
        else
        {
            Serial.println("[StorageManager] SPIFFS mount FAILED — WAV temp buffering unavailable.");
        }

        m_mountTimeMs = millis() - startMs;

        initializeDirectories();
        performAutomaticCleanup();
        printStorageSummary();

        Serial.printf("[StorageManager] Heap after init: %u bytes (delta: %+d bytes)\n",
                      ESP.getFreeHeap(),
                      (int32_t)ESP.getFreeHeap() - (int32_t)heapBefore);
        Serial.println("=================================");
        return m_spiffsMounted;
    }

    // -----------------------------------------------------------------------
    // initializeDirectories() — minimal temp dirs on SPIFFS
    // -----------------------------------------------------------------------

    void StorageManager::initializeDirectories()
    {
        if (!m_spiffsMounted) return;

        const char* dirs[] = { "/recordings", "/temp", "/logs" };
        for (const char* d : dirs)
        {
            if (!SPIFFS.exists(d))
            {
                if (SPIFFS.mkdir(d))
                    Serial.printf("[StorageManager] Created %s on SPIFFS\n", d);
                else
                    Serial.printf("[StorageManager] Failed to create %s on SPIFFS\n", d);
            }
        }
    }

    // -----------------------------------------------------------------------
    // runDiagnostics() — SPIFFS only
    // -----------------------------------------------------------------------

    StorageDiagnostics StorageManager::runDiagnostics()
    {
        StorageDiagnostics diag;
        if (!m_spiffsMounted) return diag;

        size_t total = SPIFFS.totalBytes();
        size_t used  = SPIFFS.usedBytes();

        diag.capacityMB   = total / (1024 * 1024);
        diag.usedSpaceMB  = used  / (1024 * 1024);
        diag.freeSpaceMB  = (total > used) ? (total - used) / (1024 * 1024) : 0;

        // Quick write/read/delete health check
        const char* testPath = "/temp/diag.tmp";
        {
            SpiffsLock lock("StorageManager::runDiagnostics");
            File wf = SPIFFS.open(testPath, "w");
            if (wf) { wf.print("VOXA"); wf.close(); diag.writeTestPassed = true; }

            File rf = SPIFFS.open(testPath, "r");
            if (rf && rf.readString() == "VOXA") { rf.close(); diag.readTestPassed = true; }
            else if (rf) rf.close();

            if (SPIFFS.remove(testPath)) diag.deleteTestPassed = true;
        }
        return diag;
    }

    // -----------------------------------------------------------------------
    // printStorageSummary()
    // -----------------------------------------------------------------------

    void StorageManager::printStorageSummary()
    {
        Serial.println("=================================");
        Serial.println("[StorageManager] VOXA Storage Status");
        Serial.println("[StorageManager] Mode         : Cloud-Primary (SPIFFS temp only)");
        Serial.println("[StorageManager] SD Card       : REMOVED");
        if (m_spiffsMounted)
        {
            size_t total = SPIFFS.totalBytes();
            size_t used  = SPIFFS.usedBytes();
            Serial.printf("[StorageManager] SPIFFS Total  : %u KB\n", (unsigned)(total / 1024));
            Serial.printf("[StorageManager] SPIFFS Used   : %u KB\n", (unsigned)(used  / 1024));
            Serial.printf("[StorageManager] SPIFFS Free   : %u KB\n", (unsigned)((total - used) / 1024));
        }
        else
        {
            Serial.println("[StorageManager] SPIFFS        : NOT MOUNTED");
        }
        Serial.println("[StorageManager] Cloud Backend : MongoDB Atlas + AWS S3");
        Serial.println("=================================");
    }

    // -----------------------------------------------------------------------
    // performAutomaticCleanup() — evict old temp WAV files from SPIFFS
    // -----------------------------------------------------------------------

    void StorageManager::performAutomaticCleanup()
    {
        if (!m_spiffsMounted) return;

        SpiffsLock lock("StorageManager::performAutomaticCleanup");

        // Remove any leftover temp/pending WAV files from a previous session
        File root = SPIFFS.open("/recordings");
        if (!root || !root.isDirectory()) return;

        File f = root.openNextFile();
        int  cleaned = 0;
        while (f)
        {
            String name = f.name();
            if (!f.isDirectory() && name.endsWith(".wav"))
            {
                f.close();
                if (SPIFFS.remove(name))
                {
                    Serial.printf("[StorageManager] Cleaned leftover WAV: %s\n", name.c_str());
                    cleaned++;
                }
            }
            else
            {
                f.close();
            }
            f = root.openNextFile();
        }
        root.close();

        if (cleaned > 0)
            Serial.printf("[StorageManager] Cleanup: removed %d leftover WAV files.\n", cleaned);
    }

    // -----------------------------------------------------------------------
    // File / JSON helpers — SPIFFS only
    // -----------------------------------------------------------------------

    bool StorageManager::saveRecording(const char* filename, const uint8_t* data, size_t len)
    {
        if (!m_spiffsMounted) return false;
        std::string path = resolvePath("recordings", filename);
        SpiffsLock lock("StorageManager::saveRecording");
        File f = SPIFFS.open(path.c_str(), "w");
        if (!f) return false;
        size_t written = f.write(data, len);
        f.close();
        return written == len;
    }

    bool StorageManager::saveTranscript(const char* filename, const char* text)
    {
        if (!m_spiffsMounted) return false;
        std::string path = resolvePath("transcripts", filename);
        SpiffsLock lock("StorageManager::saveTranscript");
        File f = SPIFFS.open(path.c_str(), "w");
        if (!f) return false;
        f.print(text);
        f.close();
        return true;
    }

    bool StorageManager::saveSummary(const char* filename, const char* text)
    {
        if (!m_spiffsMounted) return false;
        std::string path = resolvePath("summaries", filename);
        SpiffsLock lock("StorageManager::saveSummary");
        File f = SPIFFS.open(path.c_str(), "w");
        if (!f) return false;
        f.print(text);
        f.close();
        return true;
    }

    bool StorageManager::saveLog(const char* message)
    {
        if (!m_spiffsMounted) return false;
        SpiffsLock lock("StorageManager::saveLog");
        File f = SPIFFS.open("/logs/device.log", "a");
        if (!f) return false;
        f.println(message);
        f.close();
        return true;
    }

    bool StorageManager::saveJson(const char* path, const JsonDocument& doc)
    {
        if (!m_spiffsMounted) return false;
        SpiffsLock lock("StorageManager::saveJson");
        File f = SPIFFS.open(path, "w");
        if (!f) return false;
        serializeJson(doc, f);
        f.close();
        return true;
    }

    bool StorageManager::loadJson(const char* path, JsonDocument& doc)
    {
        if (!m_spiffsMounted) return false;
        SpiffsLock lock("StorageManager::loadJson");
        File f = SPIFFS.open(path, "r");
        if (!f) return false;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        return !err;
    }

    bool StorageManager::deleteFile(const char* path)
    {
        if (!m_spiffsMounted) return false;
        SpiffsLock lock("StorageManager::deleteFile");
        return SPIFFS.remove(path);
    }

    bool StorageManager::renameFile(const char* oldPath, const char* newPath)
    {
        if (!m_spiffsMounted) return false;
        SpiffsLock lock("StorageManager::renameFile");
        return SPIFFS.rename(oldPath, newPath);
    }

    std::vector<std::string> StorageManager::listDirectory(const char* dirPath)
    {
        std::vector<std::string> result;
        if (!m_spiffsMounted) return result;
        SpiffsLock lock("StorageManager::listDirectory");
        File root = SPIFFS.open(dirPath);
        if (!root || !root.isDirectory()) return result;
        File f = root.openNextFile();
        while (f)
        {
            result.emplace_back(f.name());
            f.close();
            f = root.openNextFile();
        }
        root.close();
        return result;
    }

    std::string StorageManager::getStorageInfoString()
    {
        if (!m_spiffsMounted) return "SPIFFS: NOT MOUNTED | Cloud: MongoDB/S3";
        size_t total = SPIFFS.totalBytes();
        size_t used  = SPIFFS.usedBytes();
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "SPIFFS: %u/%u KB | Cloud: MongoDB/S3",
                 (unsigned)(used / 1024), (unsigned)(total / 1024));
        return std::string(buf);
    }

    FS& StorageManager::getFSForPath(const char* /*path*/)
    {
        return static_cast<FS&>(SPIFFS);
    }

    const char* StorageManager::getFSNameForPath(const char* /*path*/)
    {
        return "SPIFFS";
    }

    // -----------------------------------------------------------------------
    // Capacity helpers
    // -----------------------------------------------------------------------

    uint64_t StorageManager::getTotalSpaceMB() const
    {
        return m_spiffsMounted ? SPIFFS.totalBytes() / (1024ULL * 1024ULL) : 0;
    }

    uint64_t StorageManager::getUsedSpaceMB() const
    {
        return m_spiffsMounted ? SPIFFS.usedBytes() / (1024ULL * 1024ULL) : 0;
    }

    uint64_t StorageManager::getFreeSpaceMB() const
    {
        if (!m_spiffsMounted) return 0;
        size_t t = SPIFFS.totalBytes();
        size_t u = SPIFFS.usedBytes();
        return (t > u) ? (t - u) / (1024ULL * 1024ULL) : 0;
    }

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    std::string StorageManager::resolvePath(const char* category, const char* filename)
    {
        std::string path = "/";
        path += category;
        path += "/";
        path += filename;
        return path;
    }

} // namespace VOXA