#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../models/Idea.h"
#include "../models/Memory.h"
#include "../models/Question.h"
#include "../models/Recording.h"
#include "../models/Reminder.h"

namespace VOXA
{
    class JsonStorage;

    class DataService
    {
    public:
        DataService();

        void begin();

        bool syncAll();
        bool syncReminders();
        bool syncIdeas();
        bool syncQuestions();
        bool syncOthers();
        bool syncRecordings();

        [[nodiscard]] std::vector<Reminder> getReminders();
        [[nodiscard]] std::vector<Idea> getIdeas();
        [[nodiscard]] std::vector<Question> getQuestions();
        [[nodiscard]] std::vector<Memory> getOthers();
        [[nodiscard]] std::vector<Recording> getRecordings();

        [[nodiscard]] std::size_t getReminderCount();
        [[nodiscard]] std::size_t getIdeaCount();
        [[nodiscard]] std::size_t getQuestionCount();
        [[nodiscard]] std::size_t getOtherCount();
        [[nodiscard]] std::size_t getRecordingCount();

        bool addReminderLocal(const Reminder& reminder);
        bool updateReminderLocal(const Reminder& reminder);
        bool removeReminderLocal(uint32_t id);

        bool addIdeaLocal(const Idea& idea);
        bool removeIdeaLocal(uint32_t id);

        bool addQuestionLocal(const Question& question);
        bool removeQuestionLocal(uint32_t id);

        bool addOtherLocal(const Memory& memory);
        bool updateOtherLocal(const Memory& memory);
        bool removeOtherLocal(uint32_t id);

        bool addRecordingLocal(const Recording& recording);
        bool updateRecordingLocal(const Recording& recording);
        bool removeRecordingLocal(uint32_t id);

    private:
        JsonStorage* m_cache { nullptr };
        bool m_loaded { false };

        std::vector<Reminder>  m_reminders;
        std::vector<Idea>      m_ideas;
        std::vector<Question>  m_questions;
        std::vector<Memory>    m_others;
        std::vector<Recording> m_recordings;

        bool fetchRemindersFromBackend();
        bool fetchIdeasFromBackend();
        bool fetchQuestionsFromBackend();
        bool fetchOthersFromBackend();
        bool fetchRecordingsFromBackend();

        void loadCache();
        void saveRemindersCache();
        void saveIdeasCache();
        void saveQuestionsCache();
        void saveOthersCache();
        void saveRecordingsCache();

        static uint32_t nextId(const std::vector<uint32_t>& ids);
    };

    extern DataService dataService;
}