#include "StorageService.h"
#include "../storage/JsonStorage.h"

#include <algorithm>
#include <map>
#include <string>

namespace VOXA
{
    // -----------------------------------------------------------------------
    // Static mock seed data — written to disk on first run if files are absent
    // -----------------------------------------------------------------------
    namespace
    {
        const char* kRemindersFile  = "reminders.json";
        const char* kMemoriesFile   = "memories.json";
        const char* kIdeasFile      = "ideas.json";
        const char* kQuestionsFile  = "questions.json";

        const char* kDefaultReminders = R"([
  {"id":"1","title":"Call Sofia","dateTime":"Today, 8:00 PM","completed":"false"},
  {"id":"2","title":"Emily Birthday","dateTime":"Jul 1, 2025","completed":"false"},
  {"id":"3","title":"Project Meeting","dateTime":"Tomorrow, 10:00 AM","completed":"false"}
])";
        const char* kDefaultMemories = R"([
  {"id":"1","title":"YouTube Integration","content":"","timestamp":"2 days ago","category":"voice","durationSeconds":"0"},
  {"id":"2","title":"Product Roadmap Discussion","content":"","timestamp":"3 days ago","category":"voice","durationSeconds":"0"}
])";
        const char* kDefaultIdeas = R"([
  {"id":"1","title":"YouTube Integration","content":"","timestamp":"May 28"},
  {"id":"2","title":"Offline AI Mode","content":"","timestamp":"May 27"},
  {"id":"3","title":"Smart Dashboard","content":"","timestamp":"May 26"},
  {"id":"4","title":"Auto Summary Feature","content":"","timestamp":"May 25"}
])";
        const char* kDefaultQuestions = R"([
  {"id":"1","text":"What is ChromaDB?","answer":"","timestamp":"May 28","answered":"false"},
  {"id":"2","text":"Explain LLMs simply","answer":"","timestamp":"May 27","answered":"false"},
  {"id":"3","text":"AI future predictions","answer":"","timestamp":"May 26","answered":"false"},
  {"id":"4","text":"How does memory work?","answer":"","timestamp":"May 25","answered":"false"}
])";

        /// Seed a file with default content if it doesn't exist yet.
        void seedIfEmpty(JsonStorage* s, const char* filename, const char* defaultJson)
        {
            if (s->loadJson(filename).empty())
                s->saveJson(filename, defaultJson);
        }
    }

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    StorageService::StorageService(JsonStorage* storage)
        : m_storage(storage)
    {
        // Seed mock data on first run (does nothing if files already exist).
        seedIfEmpty(m_storage, kRemindersFile,  kDefaultReminders);
        seedIfEmpty(m_storage, kMemoriesFile,   kDefaultMemories);
        seedIfEmpty(m_storage, kIdeasFile,      kDefaultIdeas);
        seedIfEmpty(m_storage, kQuestionsFile,  kDefaultQuestions);
    }

    // -----------------------------------------------------------------------
    // Private helper
    // -----------------------------------------------------------------------

    uint32_t StorageService::nextId(
        const std::vector<std::map<std::string, std::string>>& rows)
    {
        uint32_t maxId = 0;
        for (const auto& row : rows)
        {
            auto it = row.find("id");
            if (it != row.end())
            {
                try {
                    uint32_t v = static_cast<uint32_t>(std::stoul(it->second));
                    if (v > maxId) maxId = v;
                } catch (...) {}
            }
        }
        return maxId + 1;
    }

    // -----------------------------------------------------------------------
    // Reminders
    // -----------------------------------------------------------------------

    std::vector<Reminder> StorageService::loadAllReminders()
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kRemindersFile));
        std::vector<Reminder> result;
        result.reserve(rows.size());

        for (const auto& row : rows)
        {
            Reminder r;
            auto get = [&](const char* k) -> std::string {
                auto it = row.find(k);
                return it != row.end() ? it->second : std::string{};
            };
            try { r.id = static_cast<uint32_t>(std::stoul(get("id"))); } catch(...) {}
            r.title     = get("title");
            r.dateTime  = get("dateTime");
            r.completed = (get("completed") == "true");
            if (r.isValid()) result.push_back(std::move(r));
        }
        return result;
    }

    bool StorageService::saveReminder(const Reminder& reminder)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kRemindersFile));

        // Find existing entry and update, or append new one.
        bool found = false;
        for (auto& row : rows)
        {
            auto it = row.find("id");
            if (it != row.end() && it->second == std::to_string(reminder.id))
            {
                row["title"]     = reminder.title;
                row["dateTime"]  = reminder.dateTime;
                row["completed"] = reminder.completed ? "true" : "false";
                found = true;
                break;
            }
        }
        if (!found)
        {
            uint32_t id = (reminder.id == 0) ? nextId(rows) : reminder.id;
            rows.push_back({
                {"id",        std::to_string(id)},
                {"title",     reminder.title},
                {"dateTime",  reminder.dateTime},
                {"completed", reminder.completed ? "true" : "false"}
            });
        }
        return m_storage->saveJson(kRemindersFile, JsonStorage::serializeObjectArray(rows));
    }

    bool StorageService::deleteReminder(uint32_t id)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kRemindersFile));
        const std::string idStr = std::to_string(id);
        auto it = std::remove_if(rows.begin(), rows.end(),
            [&](const std::map<std::string, std::string>& r) {
                auto jt = r.find("id");
                return jt != r.end() && jt->second == idStr;
            });
        if (it == rows.end()) return false;
        rows.erase(it, rows.end());
        return m_storage->saveJson(kRemindersFile, JsonStorage::serializeObjectArray(rows));
    }

    // -----------------------------------------------------------------------
    // Memories
    // -----------------------------------------------------------------------

    std::vector<Memory> StorageService::loadAllMemories()
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kMemoriesFile));
        std::vector<Memory> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            Memory m;
            auto get = [&](const char* k) -> std::string {
                auto it = row.find(k);
                return it != row.end() ? it->second : std::string{};
            };
            try { m.id = static_cast<uint32_t>(std::stoul(get("id"))); } catch(...) {}
            m.title     = get("title");
            m.content   = get("content");
            m.timestamp = get("timestamp");
            m.category  = get("category");
            try { m.durationSeconds = static_cast<uint32_t>(std::stoul(get("durationSeconds"))); } catch(...) {}
            if (m.isValid()) result.push_back(std::move(m));
        }
        return result;
    }

    bool StorageService::saveMemory(const Memory& memory)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kMemoriesFile));
        bool found = false;
        for (auto& row : rows)
        {
            auto it = row.find("id");
            if (it != row.end() && it->second == std::to_string(memory.id))
            {
                row["title"]           = memory.title;
                row["content"]         = memory.content;
                row["timestamp"]       = memory.timestamp;
                row["category"]        = memory.category;
                row["durationSeconds"] = std::to_string(memory.durationSeconds);
                found = true;
                break;
            }
        }
        if (!found)
        {
            uint32_t id = (memory.id == 0) ? nextId(rows) : memory.id;
            rows.push_back({
                {"id",              std::to_string(id)},
                {"title",           memory.title},
                {"content",         memory.content},
                {"timestamp",       memory.timestamp},
                {"category",        memory.category},
                {"durationSeconds", std::to_string(memory.durationSeconds)}
            });
        }
        return m_storage->saveJson(kMemoriesFile, JsonStorage::serializeObjectArray(rows));
    }

    bool StorageService::deleteMemory(uint32_t id)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kMemoriesFile));
        const std::string idStr = std::to_string(id);
        auto it = std::remove_if(rows.begin(), rows.end(),
            [&](const std::map<std::string, std::string>& r) {
                auto jt = r.find("id");
                return jt != r.end() && jt->second == idStr;
            });
        if (it == rows.end()) return false;
        rows.erase(it, rows.end());
        return m_storage->saveJson(kMemoriesFile, JsonStorage::serializeObjectArray(rows));
    }

    // -----------------------------------------------------------------------
    // Ideas
    // -----------------------------------------------------------------------

    std::vector<Idea> StorageService::loadAllIdeas()
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kIdeasFile));
        std::vector<Idea> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            Idea idea;
            auto get = [&](const char* k) -> std::string {
                auto it = row.find(k);
                return it != row.end() ? it->second : std::string{};
            };
            try { idea.id = static_cast<uint32_t>(std::stoul(get("id"))); } catch(...) {}
            idea.title     = get("title");
            idea.content   = get("content");
            idea.timestamp = get("timestamp");
            if (idea.isValid()) result.push_back(std::move(idea));
        }
        return result;
    }

    bool StorageService::saveIdea(const Idea& idea)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kIdeasFile));
        bool found = false;
        for (auto& row : rows)
        {
            auto it = row.find("id");
            if (it != row.end() && it->second == std::to_string(idea.id))
            {
                row["title"]     = idea.title;
                row["content"]   = idea.content;
                row["timestamp"] = idea.timestamp;
                found = true;
                break;
            }
        }
        if (!found)
        {
            uint32_t id = (idea.id == 0) ? nextId(rows) : idea.id;
            rows.push_back({
                {"id",        std::to_string(id)},
                {"title",     idea.title},
                {"content",   idea.content},
                {"timestamp", idea.timestamp}
            });
        }
        return m_storage->saveJson(kIdeasFile, JsonStorage::serializeObjectArray(rows));
    }

    bool StorageService::deleteIdea(uint32_t id)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kIdeasFile));
        const std::string idStr = std::to_string(id);
        auto it = std::remove_if(rows.begin(), rows.end(),
            [&](const std::map<std::string, std::string>& r) {
                auto jt = r.find("id");
                return jt != r.end() && jt->second == idStr;
            });
        if (it == rows.end()) return false;
        rows.erase(it, rows.end());
        return m_storage->saveJson(kIdeasFile, JsonStorage::serializeObjectArray(rows));
    }

    // -----------------------------------------------------------------------
    // Questions
    // -----------------------------------------------------------------------

    std::vector<Question> StorageService::loadAllQuestions()
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kQuestionsFile));
        std::vector<Question> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            Question q;
            auto get = [&](const char* k) -> std::string {
                auto it = row.find(k);
                return it != row.end() ? it->second : std::string{};
            };
            try { q.id = static_cast<uint32_t>(std::stoul(get("id"))); } catch(...) {}
            q.text      = get("text");
            q.answer    = get("answer");
            q.timestamp = get("timestamp");
            q.answered  = (get("answered") == "true");
            if (q.isValid()) result.push_back(std::move(q));
        }
        return result;
    }

    bool StorageService::saveQuestion(const Question& question)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kQuestionsFile));
        bool found = false;
        for (auto& row : rows)
        {
            auto it = row.find("id");
            if (it != row.end() && it->second == std::to_string(question.id))
            {
                row["text"]      = question.text;
                row["answer"]    = question.answer;
                row["timestamp"] = question.timestamp;
                row["answered"]  = question.answered ? "true" : "false";
                found = true;
                break;
            }
        }
        if (!found)
        {
            uint32_t id = (question.id == 0) ? nextId(rows) : question.id;
            rows.push_back({
                {"id",        std::to_string(id)},
                {"text",      question.text},
                {"answer",    question.answer},
                {"timestamp", question.timestamp},
                {"answered",  question.answered ? "true" : "false"}
            });
        }
        return m_storage->saveJson(kQuestionsFile, JsonStorage::serializeObjectArray(rows));
    }

    bool StorageService::deleteQuestion(uint32_t id)
    {
        auto rows = JsonStorage::parseObjectArray(m_storage->loadJson(kQuestionsFile));
        const std::string idStr = std::to_string(id);
        auto it = std::remove_if(rows.begin(), rows.end(),
            [&](const std::map<std::string, std::string>& r) {
                auto jt = r.find("id");
                return jt != r.end() && jt->second == idStr;
            });
        if (it == rows.end()) return false;
        rows.erase(it, rows.end());
        return m_storage->saveJson(kQuestionsFile, JsonStorage::serializeObjectArray(rows));
    }
}
