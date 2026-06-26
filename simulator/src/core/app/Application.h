#pragma once

#include <memory>
#include <string>

// Forward declarations — keep this header cheap to include.
namespace VOXA
{
    class Platform;
    class JsonStorage;
    class StorageService;
    class BatteryService;
    class TimeService;
    class ReminderService;
    class IdeaService;
    class QuestionService;
    class SettingsService;
    class HistoryService;
    class RecordingService;
    class SearchService;
    class ScreenManager;
    class MemoryStorage;
    class MemoryService;

    /// Aggregated view of all application services.
    /// Screens should receive a const reference to this struct.
    struct Services
    {
        BatteryService*   battery       { nullptr };
        TimeService*      time          { nullptr };
        StorageService*   storage       { nullptr };
        ReminderService*  reminders     { nullptr };
        IdeaService*      ideas         { nullptr };
        QuestionService*  questions     { nullptr };
        SettingsService*  settings      { nullptr };
        HistoryService*   history       { nullptr };
        RecordingService* recordings    { nullptr };
        SearchService*    search        { nullptr };
        ScreenManager*    navigation    { nullptr };
        MemoryService*    memoryService { nullptr };
    };

    /// Top-level application orchestrator (new architecture).
    ///
    /// Owns every service and the platform abstraction.
    /// The existing SDL/renderer Application lives in src/core/Application.h
    /// and is not replaced — VoxaApp is the clean-architecture complement
    /// that screens will gradually migrate to.
    ///
    /// Usage:
    ///   VoxaApp app;
    ///   app.initialize();
    ///   auto& svc = app.services();
    ///   svc.reminders->getAll();
    class VoxaApp
    {
    public:
        VoxaApp();
        ~VoxaApp();

        // Non-copyable, non-movable (owns unique_ptrs).
        VoxaApp(const VoxaApp&)            = delete;
        VoxaApp& operator=(const VoxaApp&) = delete;

        /// Create and wire up all services.
        /// Call once before calling services().
        bool initialize();

        /// Cleanly shut down all services and release resources.
        void shutdown();

        /// Returns a reference to the aggregated services struct.
        /// Valid only after a successful initialize().
        [[nodiscard]] Services& services();
        [[nodiscard]] const Services& services() const;

        /// Returns the platform name, e.g. "simulator".
        [[nodiscard]] std::string platformName() const;

    private:
        // Owned objects (creation order = dependency order)
        std::unique_ptr<Platform>        m_platform;
        std::unique_ptr<JsonStorage>     m_storage;
        std::unique_ptr<StorageService>  m_storageService;
        std::unique_ptr<BatteryService>  m_batteryService;
        std::unique_ptr<TimeService>     m_timeService;
        std::unique_ptr<ReminderService> m_reminderService;
        std::unique_ptr<IdeaService>     m_ideaService;
        std::unique_ptr<QuestionService> m_questionService;
        std::unique_ptr<SettingsService> m_settingsService;
        std::unique_ptr<HistoryService>  m_historyService;
        std::unique_ptr<RecordingService> m_recordingService;
        std::unique_ptr<SearchService>   m_searchService;
        std::unique_ptr<MemoryStorage>  m_memoryStorage;
        std::unique_ptr<MemoryService>  m_memoryService;
        std::unique_ptr<ScreenManager>   m_screenManager;

        Services m_services;
        bool     m_initialized { false };
    };
}
