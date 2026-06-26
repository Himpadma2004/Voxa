#pragma once

#include <string>

namespace VOXA
{
    struct Settings
    {
        std::string theme           { "light" };
        std::string language        { "en" };
        bool        notifications   { true };
        bool        autoSync        { true };
        uint32_t    syncInterval    { 300 };
        bool        wifiEnabled     { true };
        uint32_t    storageLimit    { 32768 };
        std::string deviceName      { "VOXA Device" };
        std::string firmwareVersion { "1.0.0" };
        std::string lastSyncTime    { "" };
    };
}
