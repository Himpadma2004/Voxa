#include "Application.h"

#include "../platform/SimulatorPlatform.h"
#include "../storage/JsonStorage.h"
#include "../services/BatteryService.h"
#include "../services/TimeService.h"
#include "../services/StorageService.h"
#include "../services/ReminderService.h"
#include "../services/SearchService.h"
#include "../navigation/ScreenManager.h"
#include "../Screen.h"

namespace VOXA
{
    VoxaApp::VoxaApp()  = default;
    VoxaApp::~VoxaApp() { shutdown(); }

    bool VoxaApp::initialize()
    {
        if (m_initialized) return true;

        // 1. Platform (simulator implementation)
        auto platform = std::make_unique<SimulatorPlatform>();
        const std::string storagePath = platform->getStoragePath();

        // 2. Storage backend
        auto storage = std::make_unique<JsonStorage>(storagePath);

        // 3. Services (dependency order)
        auto storageService  = std::make_unique<StorageService>(storage.get());
        auto batteryService  = std::make_unique<BatteryService>(platform.get());
        auto timeService     = std::make_unique<TimeService>();
        auto reminderService = std::make_unique<ReminderService>(storageService.get());
        auto searchService   = std::make_unique<SearchService>(storageService.get());

        // 4. Navigation
        auto screenManager   = std::make_unique<ScreenManager>(ScreenId::Boot);

        // Populate the flat services view
        m_services.battery    = batteryService.get();
        m_services.time       = timeService.get();
        m_services.storage    = storageService.get();
        m_services.reminders  = reminderService.get();
        m_services.search     = searchService.get();
        m_services.navigation = screenManager.get();

        // Transfer ownership
        m_platform        = std::move(platform);
        m_storage         = std::move(storage);
        m_storageService  = std::move(storageService);
        m_batteryService  = std::move(batteryService);
        m_timeService     = std::move(timeService);
        m_reminderService = std::move(reminderService);
        m_searchService   = std::move(searchService);
        m_screenManager   = std::move(screenManager);

        m_initialized = true;
        return true;
    }

    void VoxaApp::shutdown()
    {
        if (!m_initialized) return;

        // Destroy in reverse dependency order
        m_screenManager.reset();
        m_searchService.reset();
        m_reminderService.reset();
        m_timeService.reset();
        m_batteryService.reset();
        m_storageService.reset();
        m_storage.reset();
        m_platform.reset();

        m_services     = {};
        m_initialized  = false;
    }

    Services& VoxaApp::services()
    {
        return m_services;
    }

    const Services& VoxaApp::services() const
    {
        return m_services;
    }

    std::string VoxaApp::platformName() const
    {
        if (m_platform) return m_platform->platformName();
        return "unknown";
    }
}
