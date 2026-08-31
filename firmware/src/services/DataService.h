#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../models/Idea.h"
#include "../models/Memory.h"
#include "../models/Question.h"
#include "../models/Recording.h"
#include "../models/Reminder.h"
#include "../models/Task.h"

// NOTE: JsonStorage / SPIFFS cache completely removed.
// DataService is now cloud-only — all data lives in MongoDB (via the Python
// backend API). In-RAM vectors are populated by syncAll() and updated
// optimistically by the add/remove/update local helpers.

namespace VOXA
{
    class DataService
    {
    public:
        DataService() = default;

        static std::string formatReadableTimestamp(const std::string& value);
        static std::string formatCountdownTimer(const std::string& value);
        static time_t      parseTimestampToEpoch(const std::string& value);

        /// Called once at startup — triggers first cloud sync.
        void begin();

        // --- Cloud sync ---
        bool syncAll();
        bool syncReminders();
        bool syncIdeas();
        bool syncQuestions();
        bool syncTasks();
        bool syncOthers();
        bool syncRecordings();

        // --- In-RAM accessors (sorted for display) ---
        [[nodiscard]] std::vector<Reminder>  getReminders();
        [[nodiscard]] std::vector<Idea>      getIdeas();
        [[nodiscard]] std::vector<Question>  getQuestions();
        [[nodiscard]] std::vector<TaskItem>  getTasks();
        [[nodiscard]] std::vector<Memory>    getOthers();
        [[nodiscard]] std::vector<Recording> getRecordings();

        [[nodiscard]] std::size_t getReminderCount();
        [[nodiscard]] std::size_t getIdeaCount();
        [[nodiscard]] std::size_t getQuestionCount();
        [[nodiscard]] std::size_t getTaskCount();
        [[nodiscard]] std::size_t getOtherCount();
        [[nodiscard]] std::size_t getRecordingCount();

        // --- Optimistic in-RAM mutations (no disk write) ---
        bool addReminderLocal(const Reminder& reminder);
        bool updateReminderLocal(const Reminder& reminder);
        bool removeReminderLocal(uint32_t id);

        bool addIdeaLocal(const Idea& idea);
        bool removeIdeaLocal(uint32_t id);

        bool addQuestionLocal(const Question& question);
        bool removeQuestionLocal(uint32_t id);

        bool addTaskLocal(const TaskItem& task);
        bool removeTaskLocal(uint32_t id);
        bool toggleTaskDone(uint32_t id);

        bool addOtherLocal(const Memory& memory);
        bool updateOtherLocal(const Memory& memory);
        bool removeOtherLocal(uint32_t id);

        bool addRecordingLocal(const Recording& recording);
        bool updateRecordingLocal(const Recording& recording);
        bool removeRecordingLocal(uint32_t id);

        bool moveItem(const std::string& fromCategory, const std::string& toCategory, uint32_t id);
        bool togglePin(const std::string& category, uint32_t id);

    private:
        bool m_loaded { false };

        std::vector<Reminder>  m_reminders;
        std::vector<Idea>      m_ideas;
        std::vector<Question>  m_questions;
        std::vector<TaskItem>  m_tasks;
        std::vector<Memory>    m_others;
        std::vector<Recording> m_recordings;

        static uint32_t nextId(const std::vector<uint32_t>& ids);
    };

    extern DataService dataService;
}