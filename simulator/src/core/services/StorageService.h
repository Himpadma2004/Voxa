#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../models/Reminder.h"
#include "../models/Memory.h"
#include "../models/Idea.h"
#include "../models/Question.h"

namespace VOXA
{
    class JsonStorage;

    /// CRUD operations for all persistent model types.
    ///
    /// Sits between the business-logic services and the raw JsonStorage layer.
    /// All UI must go through a service (ReminderService, SearchService, etc.)
    /// rather than calling StorageService directly.
    class StorageService
    {
    public:
        /// @param storage  Non-owning pointer to the storage backend.
        explicit StorageService(JsonStorage* storage);

        // ---------------------------------------------------------------
        // Reminders
        // ---------------------------------------------------------------
        [[nodiscard]] std::vector<Reminder> loadAllReminders();
        bool saveReminder(const Reminder& reminder);
        bool deleteReminder(uint32_t id);

        // ---------------------------------------------------------------
        // Memories
        // ---------------------------------------------------------------
        [[nodiscard]] std::vector<Memory> loadAllMemories();
        bool saveMemory(const Memory& memory);
        bool deleteMemory(uint32_t id);

        // ---------------------------------------------------------------
        // Ideas
        // ---------------------------------------------------------------
        [[nodiscard]] std::vector<Idea> loadAllIdeas();
        bool saveIdea(const Idea& idea);
        bool deleteIdea(uint32_t id);

        // ---------------------------------------------------------------
        // Questions
        // ---------------------------------------------------------------
        [[nodiscard]] std::vector<Question> loadAllQuestions();
        bool saveQuestion(const Question& question);
        bool deleteQuestion(uint32_t id);

    private:
        JsonStorage* m_storage { nullptr };

        // Helper: next auto-increment id for a collection.
        static uint32_t nextId(const std::vector<std::map<std::string, std::string>>& rows);
    };
}
