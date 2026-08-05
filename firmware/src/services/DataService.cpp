#include "DataService.h"
#include "ApiClient.h"
#include "../storage/JsonStorage.h"

#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <algorithm>

namespace
{
    constexpr std::size_t kPageSize = 20;

    const char* kRemindersCache  = "cache_reminders.json";
    const char* kIdeasCache      = "cache_ideas.json";
    const char* kQuestionsCache  = "cache_questions.json";
    const char* kTasksCache      = "cache_tasks.json";
    const char* kOthersCache     = "cache_others.json";
    const char* kRecordingsCache = "cache_recordings.json";

    std::string buildPagedEndpoint(const std::string& base, std::size_t skip, std::size_t limit)
    {
        std::string endpoint = base;
        endpoint += (base.find('?') == std::string::npos) ? "?" : "&";
        endpoint += "skip=" + std::to_string(skip);
        endpoint += "&limit=" + std::to_string(limit);
        return endpoint;
    }

    std::string getJsonString(JsonVariantConst value)
    {
        if (value.is<const char*>()) return value.as<const char*>();
        if (value.is<String>()) return value.as<String>().c_str();
        if (value.is<long>()) return std::to_string(value.as<long>());
        if (value.is<int>()) return std::to_string(value.as<int>());
        if (value.is<unsigned long>()) return std::to_string(value.as<unsigned long>());
        if (value.is<bool>()) return value.as<bool>() ? "true" : "false";
        if (value.isNull()) return "";
        std::string out;
        serializeJson(value, out);
        return out;
    }

    std::string normalizeTimestamp(const std::string& value)
    {
        if (value.empty()) return "";

        int year = 0, month = 0, day = 0, hour = 0, minute = 0;
        if (sscanf(value.c_str(), "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) >= 3 ||
            sscanf(value.c_str(), "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) >= 3)
        {
            static const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const char* monStr = (month >= 1 && month <= 12) ? months[month] : "";

            std::string ampm = (hour >= 12) ? "PM" : "AM";
            int h12 = hour % 12;
            if (h12 == 0) h12 = 12;

            char buf[64];
            if (monStr[0] != '\0')
            {
                snprintf(buf, sizeof(buf), "%s %d, %d:%02d %s", monStr, day, h12, minute, ampm.c_str());
            }
            else
            {
                snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
            }
            return std::string(buf);
        }
        return value;
    }

    template <typename T, typename Fn>
    bool fetchPagedList(const std::string& endpointBase, std::vector<T>& target, Fn parser)
    {
        target.clear();
        std::size_t skip = 0;

        while (true)
        {
            VOXA::ApiResult result = VOXA::apiClient.get(buildPagedEndpoint(endpointBase, skip, kPageSize));

            if (result.httpCode == 0)
            {
                Serial.printf("[DataService] Backend not responding for %s\n", endpointBase.c_str());
                return false;
            }
            
            // 1. Verify HTTP status is 200
            if (result.httpCode != 200)
            {
                Serial.printf("[DataService] Verify failed: HTTP code is not 200 (Got: %d) for %s\n", result.httpCode, endpointBase.c_str());
                return false;
            }

            // 2. Verify response body is not empty
            if (result.body.empty())
            {
                Serial.printf("[DataService] Verify failed: Response body is empty for %s\n", endpointBase.c_str());
                return false;
            }

            // 3. Verify Content-Type is application/json
            if (result.contentType.find("application/json") == std::string::npos)
            {
                Serial.printf("[DataService] Verify failed: Content-Type is not application/json (Got: %s) for %s\n", result.contentType.c_str(), endpointBase.c_str());
                return false;
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, result.body.c_str());
            if (err)
            {
                Serial.printf("[DataService] JSON parse failed for %s: %s\n", endpointBase.c_str(), err.c_str());
                Serial.printf("[DataService] Raw response body: %s\n", result.body.c_str());
                return false;
            }

            JsonArray items = doc["items"].as<JsonArray>();
            if (items.isNull())
            {
                Serial.printf("[DataService] Missing items array for %s\n", endpointBase.c_str());
                return false;
            }

            for (JsonVariantConst item : items)
            {
                T parsed{};
                parser(item, parsed);
                target.push_back(std::move(parsed));
            }

            bool hasMore = doc["page"]["has_more"] | false;
            if (!hasMore || items.size() == 0)
            {
                break;
            }

            skip += kPageSize;
        }

        return true;
    }

    template <typename T>
    uint32_t nextSequentialId(const std::vector<T>& items)
    {
        uint32_t maxId = 0;
        for (const auto& item : items)
        {
            if (item.id > maxId)
            {
                maxId = item.id;
            }
        }
        return maxId + 1;
    }
}

namespace VOXA
{
    DataService dataService;

    DataService::DataService()
        : m_cache(new JsonStorage("/voxa-cache"))
    {
    }

    std::string DataService::formatReadableTimestamp(const std::string& value)
    {
        return normalizeTimestamp(value);
    }

    void DataService::begin()
    {
        loadCache();
        m_loaded = true;
    }

    void DataService::loadCache()
    {
        auto reminderRows  = JsonStorage::parseObjectArray(m_cache->loadJson(kRemindersCache));
        auto ideaRows      = JsonStorage::parseObjectArray(m_cache->loadJson(kIdeasCache));
        auto questionRows  = JsonStorage::parseObjectArray(m_cache->loadJson(kQuestionsCache));
        auto otherRows     = JsonStorage::parseObjectArray(m_cache->loadJson(kOthersCache));
        auto recordingRows = JsonStorage::parseObjectArray(m_cache->loadJson(kRecordingsCache));

        m_reminders.clear();
        m_ideas.clear();
        m_questions.clear();
        m_others.clear();
        m_recordings.clear();

        for (const auto& row : reminderRows)
        {
            Reminder item;
            try { item.id = static_cast<uint32_t>(std::stoul(row.at("id"))); } catch (...) { continue; }
            item.title = row.count("title") ? row.at("title") : "";
            item.dateTime = row.count("dateTime") ? row.at("dateTime") : "";
            item.completed = row.count("completed") && row.at("completed") == "true";
            item.comments = row.count("comments") ? row.at("comments") : "";
            item.pinned = row.count("pinned") && row.at("pinned") == "true";
            if (item.isValid()) m_reminders.push_back(std::move(item));
        }

        for (const auto& row : ideaRows)
        {
            Idea item;
            try { item.id = static_cast<uint32_t>(std::stoul(row.at("id"))); } catch (...) { continue; }
            item.title = row.count("title") ? row.at("title") : "";
            item.content = row.count("content") ? row.at("content") : "";
            item.timestamp = row.count("timestamp") ? row.at("timestamp") : "";
            item.comments = row.count("comments") ? row.at("comments") : "";
            item.pinned = row.count("pinned") && row.at("pinned") == "true";
            if (item.isValid()) m_ideas.push_back(std::move(item));
        }

        for (const auto& row : questionRows)
        {
            Question item;
            try { item.id = static_cast<uint32_t>(std::stoul(row.at("id"))); } catch (...) { continue; }
            item.text = row.count("text") ? row.at("text") : "";
            item.answer = row.count("answer") ? row.at("answer") : "";
            item.timestamp = row.count("timestamp") ? row.at("timestamp") : "";
            item.answered = row.count("answered") && row.at("answered") == "true";
            item.comments = row.count("comments") ? row.at("comments") : "";
            item.pinned = row.count("pinned") && row.at("pinned") == "true";
            if (item.isValid()) m_questions.push_back(std::move(item));
        }

        for (const auto& row : otherRows)
        {
            Memory item;
            try { item.id = static_cast<uint32_t>(std::stoul(row.at("id"))); } catch (...) { continue; }
            item.title = row.count("title") ? row.at("title") : "";
            item.content = row.count("content") ? row.at("content") : "";
            item.timestamp = row.count("timestamp") ? row.at("timestamp") : "";
            item.createdAt = item.timestamp;
            item.updatedAt = item.timestamp;
            item.category = row.count("category") ? row.at("category") : "note";
            item.tags = row.count("tags") ? row.at("tags") : "";
            item.source = row.count("source") ? row.at("source") : "AI";
            item.comments = row.count("comments") ? row.at("comments") : "";
            item.pinned = row.count("pinned") && row.at("pinned") == "true";

            std::string catLower = item.category;
            std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
            if (catLower == "idea" || catLower == "question" || catLower == "task" || catLower == "reminder")
            {
                continue;
            }

            if (item.isValid()) m_others.push_back(std::move(item));
        }

        for (const auto& row : recordingRows)
        {
            Recording item;
            try { item.id = static_cast<uint32_t>(std::stoul(row.at("id"))); } catch (...) { continue; }
            item.title = row.count("title") ? row.at("title") : "";
            item.filePath = row.count("filePath") ? row.at("filePath") : "";
            try { item.durationSeconds = row.count("durationSeconds") ? static_cast<uint32_t>(std::stoul(row.at("durationSeconds"))) : 0; } catch (...) { item.durationSeconds = 0; }
            item.timestamp = row.count("timestamp") ? row.at("timestamp") : "";
            if (item.isValid()) m_recordings.push_back(std::move(item));
        }
    }

    bool DataService::syncAll()
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[DataService] Sync skipped: Wi-Fi not connected");
            return false;
        }

        if (!apiClient.isHealthy())
        {
            Serial.println("[DataService] Sync skipped: backend not responding");
            return false;
        }

        bool r1 = syncReminders();
        bool r2 = syncIdeas();
        bool r3 = syncQuestions();
        bool r4 = syncTasks();
        bool r5 = syncOthers();
        bool r6 = syncRecordings();
        return (r1 || r2 || r3 || r4 || r5 || r6);
    }

    bool DataService::syncReminders()
    {
        std::vector<Reminder> fetched;
        bool ok = fetchPagedList<Reminder>("/api/reminders", fetched, [](JsonVariantConst item, Reminder& out) {
            out.id = item["id"] | 0;
            out.title = getJsonString(item["title"]);
            out.dateTime = normalizeTimestamp(getJsonString(item["dateTime"]));
            out.completed = item["completed"] | false;
            out.comments = getJsonString(item["comments"]);
        });
        if (ok)
        {
            m_reminders = std::move(fetched);
            saveRemindersCache();
        }
        return ok;
    }

    bool DataService::syncIdeas()
    {
        std::vector<Idea> fetched;
        bool ok = fetchPagedList<Idea>("/api/notes?category=ideas", fetched, [](JsonVariantConst item, Idea& out) {
            out.id = item["id"] | 0;
            out.title = getJsonString(item["title"]);
            out.content = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.comments = getJsonString(item["comments"]);
        });
        if (ok)
        {
            m_ideas = std::move(fetched);
            saveIdeasCache();
        }
        return ok;
    }

    bool DataService::syncQuestions()
    {
        std::vector<Question> fetched;
        bool ok = fetchPagedList<Question>("/api/notes?category=questions", fetched, [](JsonVariantConst item, Question& out) {
            out.id = item["id"] | 0;
            out.text = getJsonString(item["title"]);
            out.answer = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.answered = !out.answer.empty();
            out.comments = getJsonString(item["comments"]);
        });
        if (ok)
        {
            m_questions = std::move(fetched);
            saveQuestionsCache();
        }
        return ok;
    }

    bool DataService::syncTasks()
    {
        std::vector<TaskItem> fetched;
        bool ok = fetchPagedList<TaskItem>("/api/notes?category=tasks", fetched, [](JsonVariantConst item, TaskItem& out) {
            out.id = item["id"] | 0;
            out.title = getJsonString(item["title"]);
            out.content = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.comments = getJsonString(item["comments"]);
            out.sourceId = getJsonString(item["source_id"]);
            out.isDone = item["completed"] | false;
        });
        if (ok)
        {
            m_tasks = std::move(fetched);
            saveTasksCache();
        }
        return ok;
    }

    bool DataService::syncOthers()
    {
        std::vector<Memory> fetched;
        bool ok = fetchPagedList<Memory>("/api/notes?category=others", fetched, [](JsonVariantConst item, Memory& out) {
            out.id = item["id"] | 0;
            out.title = getJsonString(item["title"]);
            out.content = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.createdAt = out.timestamp;
            out.updatedAt = out.timestamp;
            out.category = getJsonString(item["category"]);
            out.source = "AI";
            out.comments = getJsonString(item["comments"]);
        });
        if (ok)
        {
            std::vector<Memory> filtered;
            for (auto& m : fetched)
            {
                std::string catLower = m.category;
                std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                if (catLower == "idea" || catLower == "question" || catLower == "task" || catLower == "reminder")
                {
                    continue;
                }
                filtered.push_back(std::move(m));
            }
            m_others = std::move(filtered);
            saveOthersCache();
        }
        return ok;
    }

    bool DataService::syncRecordings()
    {
        std::vector<Recording> fetched;
        bool ok = fetchPagedList<Recording>("/api/notes?category=recordings", fetched, [](JsonVariantConst item, Recording& out) {
            out.id = item["id"] | 0;
            out.title = getJsonString(item["title"]);
            out.filePath = getJsonString(item["filePath"]);
            out.durationSeconds = item["durationSeconds"] | 0;
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
        });
        if (ok)
        {
            m_recordings = std::move(fetched);
            saveRecordingsCache();
        }
        return ok;
    }

    std::vector<Reminder> DataService::getReminders()
    {
        if (!m_loaded) begin();
        auto copy = m_reminders;
        std::sort(copy.begin(), copy.end(), [](const Reminder& a, const Reminder& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            return a.id > b.id;
        });
        return copy;
    }

    std::vector<Idea> DataService::getIdeas()
    {
        if (!m_loaded) begin();
        auto copy = m_ideas;
        std::sort(copy.begin(), copy.end(), [](const Idea& a, const Idea& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            return a.id > b.id;
        });
        return copy;
    }

    std::vector<Question> DataService::getQuestions()
    {
        if (!m_loaded) begin();
        auto copy = m_questions;
        std::sort(copy.begin(), copy.end(), [](const Question& a, const Question& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            return a.id > b.id;
        });
        return copy;
    }

    std::vector<TaskItem> DataService::getTasks()
    {
        if (!m_loaded) begin();
        auto copy = m_tasks;
        std::sort(copy.begin(), copy.end(), [](const TaskItem& a, const TaskItem& b) {
            if (a.isPinned != b.isPinned) return a.isPinned > b.isPinned;
            return a.id > b.id;
        });
        return copy;
    }

    std::vector<Memory> DataService::getOthers()
    {
        if (!m_loaded) begin();
        auto copy = m_others;
        std::sort(copy.begin(), copy.end(), [](const Memory& a, const Memory& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            return a.id > b.id;
        });
        return copy;
    }

    std::vector<Recording> DataService::getRecordings()
    {
        if (!m_loaded) begin();
        auto copy = m_recordings;
        std::sort(copy.begin(), copy.end(), [](const Recording& a, const Recording& b) {
            return a.id > b.id;
        });
        return copy;
    }

    std::size_t DataService::getReminderCount() { return getReminders().size(); }
    std::size_t DataService::getIdeaCount() { return getIdeas().size(); }
    std::size_t DataService::getQuestionCount() { return getQuestions().size(); }
    std::size_t DataService::getTaskCount() { return getTasks().size(); }
    std::size_t DataService::getOtherCount() { return getOthers().size(); }
    std::size_t DataService::getRecordingCount() { return getRecordings().size(); }

    bool DataService::addTaskLocal(const TaskItem& task)
    {
        auto copy = task;
        copy.id = nextSequentialId(m_tasks);
        m_tasks.insert(m_tasks.begin(), copy);
        saveTasksCache();
        return true;
    }

    bool DataService::removeTaskLocal(uint32_t id)
    {
        auto it = std::remove_if(m_tasks.begin(), m_tasks.end(), [id](const TaskItem& item) { return item.id == id; });
        if (it == m_tasks.end()) return false;
        m_tasks.erase(it, m_tasks.end());
        saveTasksCache();
        return true;
    }

    bool DataService::toggleTaskDone(uint32_t id)
    {
        for (auto& t : m_tasks)
        {
            if (t.id == id)
            {
                t.isDone = !t.isDone;
                saveTasksCache();
                return true;
            }
        }
        return false;
    }

    void DataService::saveTasksCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_tasks.size());
        for (const auto& item : m_tasks)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"title", item.title},
                {"content", item.content},
                {"timestamp", item.timestamp},
                {"comments", item.comments},
                {"sourceId", item.sourceId},
                {"isDone", item.isDone ? "true" : "false"},
                {"pinned", item.isPinned ? "true" : "false"}
            });
        }
        m_cache->saveJson(kTasksCache, JsonStorage::serializeObjectArray(rows));
    }

    bool DataService::addReminderLocal(const Reminder& reminder)
    {
        auto copy = reminder;
        copy.id = nextSequentialId(m_reminders);
        m_reminders.insert(m_reminders.begin(), copy);
        saveRemindersCache();
        return true;
    }

    bool DataService::updateReminderLocal(const Reminder& reminder)
    {
        for (auto& item : m_reminders)
        {
            if (item.id == reminder.id)
            {
                item = reminder;
                saveRemindersCache();
                return true;
            }
        }
        return addReminderLocal(reminder);
    }

    bool DataService::removeReminderLocal(uint32_t id)
    {
        auto it = std::remove_if(m_reminders.begin(), m_reminders.end(), [id](const Reminder& item) { return item.id == id; });
        if (it == m_reminders.end()) return false;
        m_reminders.erase(it, m_reminders.end());
        saveRemindersCache();
        return true;
    }

    bool DataService::addIdeaLocal(const Idea& idea)
    {
        auto copy = idea;
        copy.id = nextSequentialId(m_ideas);
        m_ideas.insert(m_ideas.begin(), copy);
        saveIdeasCache();
        return true;
    }

    bool DataService::removeIdeaLocal(uint32_t id)
    {
        auto it = std::remove_if(m_ideas.begin(), m_ideas.end(), [id](const Idea& item) { return item.id == id; });
        if (it == m_ideas.end()) return false;
        m_ideas.erase(it, m_ideas.end());
        saveIdeasCache();
        return true;
    }

    bool DataService::addQuestionLocal(const Question& question)
    {
        auto copy = question;
        copy.id = nextSequentialId(m_questions);
        m_questions.insert(m_questions.begin(), copy);
        saveQuestionsCache();
        return true;
    }

    bool DataService::removeQuestionLocal(uint32_t id)
    {
        auto it = std::remove_if(m_questions.begin(), m_questions.end(), [id](const Question& item) { return item.id == id; });
        if (it == m_questions.end()) return false;
        m_questions.erase(it, m_questions.end());
        saveQuestionsCache();
        return true;
    }

    bool DataService::addOtherLocal(const Memory& memory)
    {
        auto copy = memory;
        copy.id = nextSequentialId(m_others);
        m_others.insert(m_others.begin(), copy);
        saveOthersCache();
        return true;
    }

    bool DataService::updateOtherLocal(const Memory& memory)
    {
        for (auto& item : m_others)
        {
            if (item.id == memory.id)
            {
                item = memory;
                saveOthersCache();
                return true;
            }
        }
        return addOtherLocal(memory);
    }

    bool DataService::removeOtherLocal(uint32_t id)
    {
        auto it = std::remove_if(m_others.begin(), m_others.end(), [id](const Memory& item) { return item.id == id; });
        if (it == m_others.end()) return false;
        m_others.erase(it, m_others.end());
        saveOthersCache();
        return true;
    }

    bool DataService::addRecordingLocal(const Recording& recording)
    {
        auto copy = recording;
        copy.id = nextSequentialId(m_recordings);
        m_recordings.insert(m_recordings.begin(), copy);
        saveRecordingsCache();
        return true;
    }

    bool DataService::updateRecordingLocal(const Recording& recording)
    {
        for (auto& item : m_recordings)
        {
            if (item.id == recording.id)
            {
                item = recording;
                saveRecordingsCache();
                return true;
            }
        }
        return addRecordingLocal(recording);
    }

    bool DataService::removeRecordingLocal(uint32_t id)
    {
        auto it = std::remove_if(m_recordings.begin(), m_recordings.end(), [id](const Recording& item) { return item.id == id; });
        if (it == m_recordings.end()) return false;
        m_recordings.erase(it, m_recordings.end());
        saveRecordingsCache();
        return true;
    }

    void DataService::saveRemindersCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_reminders.size());
        for (const auto& item : m_reminders)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"title", item.title},
                {"dateTime", item.dateTime},
                {"completed", item.completed ? "true" : "false"},
                {"comments", item.comments},
                {"pinned", item.pinned ? "true" : "false"}
            });
        }
        m_cache->saveJson(kRemindersCache, JsonStorage::serializeObjectArray(rows));
    }

    void DataService::saveIdeasCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_ideas.size());
        for (const auto& item : m_ideas)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"title", item.title},
                {"content", item.content},
                {"timestamp", item.timestamp},
                {"comments", item.comments},
                {"pinned", item.pinned ? "true" : "false"}
            });
        }
        m_cache->saveJson(kIdeasCache, JsonStorage::serializeObjectArray(rows));
    }

    void DataService::saveQuestionsCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_questions.size());
        for (const auto& item : m_questions)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"text", item.text},
                {"answer", item.answer},
                {"timestamp", item.timestamp},
                {"answered", item.answered ? "true" : "false"},
                {"comments", item.comments},
                {"pinned", item.pinned ? "true" : "false"}
            });
        }
        m_cache->saveJson(kQuestionsCache, JsonStorage::serializeObjectArray(rows));
    }

    void DataService::saveOthersCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_others.size());
        for (const auto& item : m_others)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"title", item.title},
                {"content", item.content},
                {"timestamp", item.timestamp},
                {"category", item.category},
                {"tags", item.tags},
                {"source", item.source},
                {"comments", item.comments},
                {"pinned", item.pinned ? "true" : "false"}
            });
        }
        m_cache->saveJson(kOthersCache, JsonStorage::serializeObjectArray(rows));
    }

    void DataService::saveRecordingsCache()
    {
        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(m_recordings.size());
        for (const auto& item : m_recordings)
        {
            rows.push_back({
                {"id", std::to_string(item.id)},
                {"title", item.title},
                {"filePath", item.filePath},
                {"durationSeconds", std::to_string(item.durationSeconds)},
                {"timestamp", item.timestamp}
            });
        }
        m_cache->saveJson(kRecordingsCache, JsonStorage::serializeObjectArray(rows));
    }

    bool DataService::togglePin(const std::string& category, uint32_t id)
    {
        if (category == "reminders")
        {
            for (auto& item : m_reminders)
            {
                if (item.id == id)
                {
                    item.pinned = !item.pinned;
                    saveRemindersCache();
                    return true;
                }
            }
        }
        else if (category == "ideas")
        {
            for (auto& item : m_ideas)
            {
                if (item.id == id)
                {
                    item.pinned = !item.pinned;
                    saveIdeasCache();
                    return true;
                }
            }
        }
        else if (category == "questions")
        {
            for (auto& item : m_questions)
            {
                if (item.id == id)
                {
                    item.pinned = !item.pinned;
                    saveQuestionsCache();
                    return true;
                }
            }
        }
        else if (category == "others" || category == "memories")
        {
            for (auto& item : m_others)
            {
                if (item.id == id)
                {
                    item.pinned = !item.pinned;
                    saveOthersCache();
                    return true;
                }
            }
        }
        return false;
    }

    bool DataService::moveItem(const std::string& fromCategory, const std::string& toCategory, uint32_t id)
    {
        if (fromCategory == toCategory) return true;

        std::string title = "";
        std::string content = "";
        std::string comments = "";
        bool pinned = false;

        if (fromCategory == "reminders")
        {
            for (auto it = m_reminders.begin(); it != m_reminders.end(); ++it)
            {
                if (it->id == id)
                {
                    title = it->title;
                    content = "Reminder comments";
                    comments = it->comments;
                    pinned = it->pinned;
                    m_reminders.erase(it);
                    saveRemindersCache();
                    break;
                }
            }
        }
        else if (fromCategory == "ideas")
        {
            for (auto it = m_ideas.begin(); it != m_ideas.end(); ++it)
            {
                if (it->id == id)
                {
                    title = it->title;
                    content = it->content;
                    comments = it->comments;
                    pinned = it->pinned;
                    m_ideas.erase(it);
                    saveIdeasCache();
                    break;
                }
            }
        }
        else if (fromCategory == "questions")
        {
            for (auto it = m_questions.begin(); it != m_questions.end(); ++it)
            {
                if (it->id == id)
                {
                    title = it->text;
                    content = it->answer;
                    comments = it->comments;
                    pinned = it->pinned;
                    m_questions.erase(it);
                    saveQuestionsCache();
                    break;
                }
            }
        }
        else if (fromCategory == "others" || fromCategory == "memories")
        {
            for (auto it = m_others.begin(); it != m_others.end(); ++it)
            {
                if (it->id == id)
                {
                    title = it->title;
                    content = it->content;
                    comments = it->comments;
                    pinned = it->pinned;
                    m_others.erase(it);
                    saveOthersCache();
                    break;
                }
            }
        }

        if (title.empty()) return false;

        if (toCategory == "reminders")
        {
            Reminder item;
            item.id = nextSequentialId(m_reminders);
            item.title = title;
            item.dateTime = "2026-07-14 12:00";
            item.completed = false;
            item.comments = comments;
            item.pinned = pinned;
            m_reminders.insert(m_reminders.begin(), item);
            saveRemindersCache();
            return true;
        }
        else if (toCategory == "ideas")
        {
            Idea item;
            item.id = nextSequentialId(m_ideas);
            item.title = title;
            item.content = content;
            item.timestamp = "2026-07-14 12:00";
            item.comments = comments;
            item.pinned = pinned;
            m_ideas.insert(m_ideas.begin(), item);
            saveIdeasCache();
            return true;
        }
        else if (toCategory == "questions")
        {
            Question item;
            item.id = nextSequentialId(m_questions);
            item.text = title;
            item.answer = content;
            item.timestamp = "2026-07-14 12:00";
            item.answered = !content.empty();
            item.comments = comments;
            item.pinned = pinned;
            m_questions.insert(m_questions.begin(), item);
            saveQuestionsCache();
            return true;
        }
        else if (toCategory == "others" || toCategory == "memories")
        {
            Memory item;
            item.id = nextSequentialId(m_others);
            item.title = title;
            item.content = content;
            item.timestamp = "2026-07-14 12:00";
            item.category = "note";
            item.pinned = pinned;
            m_others.insert(m_others.begin(), item);
            saveOthersCache();
            return true;
        }

        return false;
    }

    uint32_t DataService::nextId(const std::vector<uint32_t>& ids)
    {
        uint32_t maxId = 0;
        for (uint32_t id : ids)
        {
            if (id > maxId) maxId = id;
        }
        return maxId + 1;
    }

}