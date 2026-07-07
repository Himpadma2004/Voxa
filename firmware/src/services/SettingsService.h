#pragma once

#include <string>
#include "../models/Settings.h"

namespace VOXA
{
    class StorageService;

    class SettingsService
    {
    public:
        explicit SettingsService(StorageService* storage);

        [[nodiscard]] Settings getSettings();
        bool updateSettings(const Settings& settings);

    private:
        StorageService* m_storage { nullptr };
    };
}
