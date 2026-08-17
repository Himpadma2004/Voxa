#pragma once

#include <cstddef>
#include <cstdint>

// ============================================================
// SDCardService — STUB (SD card adapter removed from hardware)
// All physical SD card storage has been migrated to cloud
// (MongoDB + AWS S3). This stub keeps the build green for any
// remaining call-sites while doing nothing at runtime.
// ============================================================

namespace VOXA
{
    class SDCardService
    {
    public:
        SDCardService()  = default;
        ~SDCardService() = default;

        bool begin()              { return false; }
        void refreshStorageInfo() {}
        void logPush(const char* /*action*/, const char* /*path*/,
                     size_t /*sizeBytes*/, const char* /*itemType*/) {}
        bool checkPresence()      { return false; }

        [[nodiscard]] bool     isCardAttached()  const { return false; }
        [[nodiscard]] bool     isMounted()       const { return false; }
        [[nodiscard]] uint64_t getCardSizeMB()   const { return 0; }
        [[nodiscard]] uint64_t getTotalSpaceMB() const { return 0; }
        [[nodiscard]] uint64_t getUsedSpaceMB()  const { return 0; }
        [[nodiscard]] uint64_t getFreeSpaceMB()  const { return 0; }
    };

    extern SDCardService sdCardService;
}
