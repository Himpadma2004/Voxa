#include "SettingsService.h"
#include "StorageService.h"

namespace VOXA
{
    SettingsService::SettingsService(StorageService* storage)
        : m_storage(storage)
    {
    }

    Settings SettingsService::getSettings()
    {
        return m_storage->loadSettings();
    }

    bool SettingsService::updateSettings(const Settings& settings)
    {
        return m_storage->saveSettings(settings);
    }
}
