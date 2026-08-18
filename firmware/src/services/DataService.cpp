#include "DataService.h"
#include "ApiClient.h"

#include <WiFi.h>
#include <ArduinoJson.h>
#include <algorithm>

// ============================================================
// DataService — Cloud-Only Storage
// All persistent data lives in MongoDB via the Python backend.
// In-RAM vectors are populated by syncAll() / sync*() calls and
// mutated optimistically by the Local helpers (no disk write).
// ============================================================

namespace
{
    constexpr std::size_t kPageSize = 20;

    std::string buildPagedEndpoint(const std::string& base, std::size_t skip, std::size_t limit)
    {
        std::string endpoint = base;
        endpoint += (base.find('?') == std::string::npos) ? "?" : "&";
        endpoint += "skip="  + std::to_string(skip);
        endpoint += "&limit=" + std::to_string(limit);
        return endpoint;
    }

    std::string getJsonString(JsonVariantConst value)
    {
        if (value.is<const char*>())   return value.as<const char*>();
        if (value.is<String>())        return value.as<String>().c_str();
        if (value.is<long>())          return std::to_string(value.as<long>());
        if (value.is<int>())           return std::to_string(value.as<int>());
        if (value.is<unsigned long>()) return std::to_string(value.as<unsigned long>());
        if (value.is<bool>())          return value.as<bool>() ? "true" : "false";
        if (value.isNull())            return "";
        std::string out;
        serializeJson(value, out);
        return out;
    }

    std::string normalizeTimestamp(const std::string& value)
    {
        if (value.empty()) return "";

        int year = 0, month = 0, day = 0, hour = 0, minute = 0;
        if (sscanf(value.c_str(), "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) >= 3 ||
            sscanf(value.c_str(), "%d-%d-%d %d:%d",  &year, &month, &day, &hour, &minute) >= 3)
        {
            static const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May",
                                           "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const char* monStr = (month >= 1 && month <= 12) ? months[month] : "";

            std::string ampm = (hour >= 12) ? "PM" : "AM";
            int h12 = hour % 12;
            if (h12 == 0) h12 = 12;

            char buf[64];
            if (monStr[0] != '\0')
                snprintf(buf, sizeof(buf), "%s %d, %d:%02d %s", monStr, day, h12, minute, ampm.c_str());
            else
                snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
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
            if (result.httpCode != 200)
            {
                Serial.printf("[DataService] HTTP %d for %s\n", result.httpCode, endpointBase.c_str());
                return false;
            }
            if (result.body.empty())
            {
                Serial.printf("[DataService] Empty response body for %s\n", endpointBase.c_str());
                return false;
            }
            if (result.contentType.find("application/json") == std::string::npos)
            {
                Serial.printf("[DataService] Non-JSON Content-Type (%s) for %s\n",
                              result.contentType.c_str(), endpointBase.c_str());
                return false;
            }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, result.body.c_str());
            if (err)
            {
                Serial.printf("[DataService] JSON parse failed for %s: %s\n",
                              endpointBase.c_str(), err.c_str());
                return false;
            }

            JsonArray items = doc["items"].as<JsonArray>();
            if (items.isNull())
            {
                Serial.printf("[DataService] Missing 'items' array for %s\n", endpointBase.c_str());
                return false;
            }

            for (JsonVariantConst item : items)
            {
                T parsed{};
                parser(item, parsed);
                target.push_back(std::move(parsed));
            }

            bool hasMore = doc["page"]["has_more"] | false;
            if (!hasMore || items.size() == 0) break;
            skip += kPageSize;
        }

        return true;
    }

    template <typename T>
    uint32_t nextSequentialId(const std::vector<T>& items)
    {
        uint32_t maxId = 0;
        for (const auto& item : items)
            if (item.id > maxId) maxId = item.id;
        return maxId + 1;
    }
}

namespace VOXA
{
    DataService dataService;

    // -----------------------------------------------------------------------
    // Static helpers
    // -----------------------------------------------------------------------

    std::string DataService::formatReadableTimestamp(const std::string& value)
    {
        return normalizeTimestamp(value);
    }

    std::string DataService::formatCountdownTimer(const std::string& value)
    {
        if (value.empty()) return "No time set";

        int year = 0, month = 0, day = 0, hour = 0, minute = 0;
        if (sscanf(value.c_str(), "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) >= 3 ||
            sscanf(value.c_str(), "%d-%d-%d %d:%d",  &year, &month, &day, &hour, &minute) >= 3)
        {
            struct tm t = {};
            t.tm_year = year - 1900;
            t.tm_mon  = month - 1;
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min  = minute;
            t.tm_sec  = 0;
            t.tm_isdst = -1;
            time_t dueTime = mktime(&t);

            time_t now = time(nullptr);
            if (now <= 100000) return normalizeTimestamp(value);

            double diffSec = difftime(dueTime, now);
            if (diffSec <= -60)
            {
                int pastMins = (int)(-diffSec / 60);
                if (pastMins < 60) return "Overdue by " + std::to_string(pastMins) + "m";
                int pastHours = pastMins / 60;
                return "Overdue by " + std::to_string(pastHours) + "h " +
                       std::to_string(pastMins % 60) + "m";
            }
            else if (diffSec < 60)
            {
                return "Due now";
            }
            else
            {
                int remMins = (int)(diffSec / 60);
                if (remMins < 60) return "In " + std::to_string(remMins) + "m";
                int remHours = remMins / 60;
                if (remHours < 24)
                    return "In " + std::to_string(remHours) + "h " + std::to_string(remMins % 60) + "m";
                int remDays  = remHours / 24;
                return "In " + std::to_string(remDays) + "d " + std::to_string(remHours % 24) + "h";
            }
        }
        return normalizeTimestamp(value);
    }

    // -----------------------------------------------------------------------
    // begin() — trigger first cloud sync instead of loading from SPIFFS
    // -----------------------------------------------------------------------

    void DataService::begin()
    {
        Serial.println("[DataService] Cloud-only mode — no local cache. Syncing from backend...");
        syncAll();
        m_loaded = true;
    }

    // -----------------------------------------------------------------------
    // Cloud sync
    // -----------------------------------------------------------------------

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
            out.id        = item["id"] | 0;
            out.title     = getJsonString(item["title"]);
            out.dateTime  = normalizeTimestamp(getJsonString(item["dateTime"]));
            out.completed = item["completed"] | false;
            out.comments  = getJsonString(item["comments"]);
        });
        if (ok) m_reminders = std::move(fetched);
        return ok;
    }

    bool DataService::syncIdeas()
    {
        std::vector<Idea> fetched;
        bool ok = fetchPagedList<Idea>("/api/notes?category=ideas", fetched, [](JsonVariantConst item, Idea& out) {
            out.id        = item["id"] | 0;
            out.title     = getJsonString(item["title"]);
            out.content   = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.comments  = getJsonString(item["comments"]);
        });
        if (ok) m_ideas = std::move(fetched);
        return ok;
    }

    bool DataService::syncQuestions()
    {
        std::vector<Question> fetched;
        bool ok = fetchPagedList<Question>("/api/notes?category=questions", fetched, [](JsonVariantConst item, Question& out) {
            out.id        = item["id"] | 0;
            out.text      = getJsonString(item["title"]);
            out.answer    = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.answered  = !out.answer.empty();
            out.comments  = getJsonString(item["comments"]);
        });
        if (ok) m_questions = std::move(fetched);
        return ok;
    }

    bool DataService::syncTasks()
    {
        std::vector<TaskItem> fetched;
        bool ok = fetchPagedList<TaskItem>("/api/notes?category=tasks", fetched, [](JsonVariantConst item, TaskItem& out) {
            out.id        = item["id"] | 0;
            out.title     = getJsonString(item["title"]);
            out.content   = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.comments  = getJsonString(item["comments"]);
            out.sourceId  = getJsonString(item["source_id"]);
            out.mongoId   = getJsonString(item["mongo_id"]);
            out.isDone    = item["completed"] | false;
        });
        if (ok) m_tasks = std::move(fetched);
        return ok;
    }

    bool DataService::syncOthers()
    {
        std::vector<Memory> fetched;
        bool ok = fetchPagedList<Memory>("/api/notes?category=others", fetched, [](JsonVariantConst item, Memory& out) {
            out.id        = item["id"] | 0;
            out.title     = getJsonString(item["title"]);
            out.content   = getJsonString(item["content"]);
            out.timestamp = normalizeTimestamp(getJsonString(item["timestamp"]));
            out.createdAt = out.timestamp;
            out.updatedAt = out.timestamp;
            out.category  = getJsonString(item["category"]);
            out.source    = "AI";
            out.comments  = getJsonString(item["comments"]);
        });
        if (ok)
        {
            std::vector<Memory> filtered;
            for (auto& m : fetched)
            {
                std::string catLower = m.category;
                std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                if (catLower == "idea" || catLower == "question" ||
                    catLower == "task" || catLower == "reminder")
                    continue;
                filtered.push_back(std::move(m));
            }
            m_others = std::move(filtered);
        }
        return ok;
    }

    bool DataService::syncRecordings()
    {
        std::vector<Recording> fetched;
        bool ok = fetchPagedList<Recording>("/api/notes?category=recordings", fetched, [](JsonVariantConst item, Recording& out) {
            out.id              = item["id"] | 0;
            out.title           = getJsonString(item["title"]);
            out.filePath        = getJsonString(item["filePath"]);
            out.durationSeconds = item["durationSeconds"] | 0;
            out.timestamp       = normalizeTimestamp(getJsonString(item["timestamp"]));
        });
        if (ok) m_recordings = std::move(fetched);
        return ok;
    }

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------
    // Accessors (sorted from latest to oldest for display)
    // -----------------------------------------------------------------------

    std::vector<Reminder> DataService::getReminders()
    {
        if (!m_loaded) begin();
        auto copy = m_reminders;
        std::sort(copy.begin(), copy.end(), [](const Reminder& a, const Reminder& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            if (!a.dateTime.empty() && !b.dateTime.empty() && a.dateTime != b.dateTime)
                return a.dateTime > b.dateTime;
            return a.id < b.id;
        });
        return copy;
    }

    std::vector<Idea> DataService::getIdeas()
    {
        if (!m_loaded) begin();
        auto copy = m_ideas;
        std::sort(copy.begin(), copy.end(), [](const Idea& a, const Idea& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            if (!a.timestamp.empty() && !b.timestamp.empty() && a.timestamp != b.timestamp)
                return a.timestamp > b.timestamp;
            return a.id < b.id;
        });
        return copy;
    }

    std::vector<Question> DataService::getQuestions()
    {
        if (!m_loaded) begin();
        auto copy = m_questions;
        std::sort(copy.begin(), copy.end(), [](const Question& a, const Question& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            if (!a.timestamp.empty() && !b.timestamp.empty() && a.timestamp != b.timestamp)
                return a.timestamp > b.timestamp;
            return a.id < b.id;
        });
        return copy;
    }

    std::vector<TaskItem> DataService::getTasks()
    {
        if (!m_loaded) begin();
        auto copy = m_tasks;
        std::sort(copy.begin(), copy.end(), [](const TaskItem& a, const TaskItem& b) {
            if (a.isPinned != b.isPinned) return a.isPinned > b.isPinned;
            if (!a.timestamp.empty() && !b.timestamp.empty() && a.timestamp != b.timestamp)
                return a.timestamp > b.timestamp;
            return a.id < b.id;
        });
        return copy;
    }

    std::vector<Memory> DataService::getOthers()
    {
        if (!m_loaded) begin();
        auto copy = m_others;
        std::sort(copy.begin(), copy.end(), [](const Memory& a, const Memory& b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            if (!a.timestamp.empty() && !b.timestamp.empty() && a.timestamp != b.timestamp)
                return a.timestamp > b.timestamp;
            return a.id < b.id;
        });
        return copy;
    }

    std::vector<Recording> DataService::getRecordings()
    {
        if (!m_loaded) begin();
        auto copy = m_recordings;
        std::sort(copy.begin(), copy.end(), [](const Recording& a, const Recording& b) {
            if (!a.timestamp.empty() && !b.timestamp.empty() && a.timestamp != b.timestamp)
                return a.timestamp > b.timestamp;
            return a.id < b.id;
        });
        return copy;
    }

    std::size_t DataService::getReminderCount()  { return getReminders().size(); }
    std::size_t DataService::getIdeaCount()      { return getIdeas().size(); }
    std::size_t DataService::getQuestionCount()  { return getQuestions().size(); }
    std::size_t DataService::getTaskCount()      { return getTasks().size(); }
    std::size_t DataService::getOtherCount()     { return getOthers().size(); }
    std::size_t DataService::getRecordingCount() { return getRecordings().size(); }

    // -----------------------------------------------------------------------
    // Optimistic in-RAM mutations (no disk write — next sync restores truth)
    // -----------------------------------------------------------------------

    bool DataService::addTaskLocal(const TaskItem& task)
    {
        auto copy = task;
        copy.id = nextSequentialId(m_tasks);
        m_tasks.insert(m_tasks.begin(), copy);
        return true;
    }

    bool DataService::removeTaskLocal(uint32_t id)
    {
        auto it = std::remove_if(m_tasks.begin(), m_tasks.end(),
                                 [id](const TaskItem& item) { return item.id == id; });
        if (it == m_tasks.end()) return false;
        m_tasks.erase(it, m_tasks.end());
        return true;
    }

    bool DataService::toggleTaskDone(uint32_t id)
    {
        for (auto& t : m_tasks)
            if (t.id == id) { t.isDone = !t.isDone; return true; }
        return false;
    }

    bool DataService::addReminderLocal(const Reminder& reminder)
    {
        auto copy = reminder;
        copy.id = nextSequentialId(m_reminders);
        m_reminders.insert(m_reminders.begin(), copy);
        return true;
    }

    bool DataService::updateReminderLocal(const Reminder& reminder)
    {
        for (auto& item : m_reminders)
            if (item.id == reminder.id) { item = reminder; return true; }
        return addReminderLocal(reminder);
    }

    bool DataService::removeReminderLocal(uint32_t id)
    {
        auto it = std::remove_if(m_reminders.begin(), m_reminders.end(),
                                 [id](const Reminder& item) { return item.id == id; });
        if (it == m_reminders.end()) return false;
        m_reminders.erase(it, m_reminders.end());
        return true;
    }

    bool DataService::addIdeaLocal(const Idea& idea)
    {
        auto copy = idea;
        copy.id = nextSequentialId(m_ideas);
        m_ideas.insert(m_ideas.begin(), copy);
        return true;
    }

    bool DataService::removeIdeaLocal(uint32_t id)
    {
        auto it = std::remove_if(m_ideas.begin(), m_ideas.end(),
                                 [id](const Idea& item) { return item.id == id; });
        if (it == m_ideas.end()) return false;
        m_ideas.erase(it, m_ideas.end());
        return true;
    }

    bool DataService::addQuestionLocal(const Question& question)
    {
        auto copy = question;
        copy.id = nextSequentialId(m_questions);
        m_questions.insert(m_questions.begin(), copy);
        return true;
    }

    bool DataService::removeQuestionLocal(uint32_t id)
    {
        auto it = std::remove_if(m_questions.begin(), m_questions.end(),
                                 [id](const Question& item) { return item.id == id; });
        if (it == m_questions.end()) return false;
        m_questions.erase(it, m_questions.end());
        return true;
    }

    bool DataService::addOtherLocal(const Memory& memory)
    {
        auto copy = memory;
        copy.id = nextSequentialId(m_others);
        m_others.insert(m_others.begin(), copy);
        return true;
    }

    bool DataService::updateOtherLocal(const Memory& memory)
    {
        for (auto& item : m_others)
            if (item.id == memory.id) { item = memory; return true; }
        return addOtherLocal(memory);
    }

    bool DataService::removeOtherLocal(uint32_t id)
    {
        auto it = std::remove_if(m_others.begin(), m_others.end(),
                                 [id](const Memory& item) { return item.id == id; });
        if (it == m_others.end()) return false;
        m_others.erase(it, m_others.end());
        return true;
    }

    bool DataService::addRecordingLocal(const Recording& recording)
    {
        auto copy = recording;
        copy.id = nextSequentialId(m_recordings);
        m_recordings.insert(m_recordings.begin(), copy);
        return true;
    }

    bool DataService::updateRecordingLocal(const Recording& recording)
    {
        for (auto& item : m_recordings)
            if (item.id == recording.id) { item = recording; return true; }
        return addRecordingLocal(recording);
    }

    bool DataService::removeRecordingLocal(uint32_t id)
    {
        auto it = std::remove_if(m_recordings.begin(), m_recordings.end(),
                                 [id](const Recording& item) { return item.id == id; });
        if (it == m_recordings.end()) return false;
        m_recordings.erase(it, m_recordings.end());
        return true;
    }

    // -----------------------------------------------------------------------
    // togglePin / moveItem — pure in-RAM, no cache write
    // -----------------------------------------------------------------------

    bool DataService::togglePin(const std::string& category, uint32_t id)
    {
        if (category == "reminders")
        {
            for (auto& item : m_reminders)
                if (item.id == id) { item.pinned = !item.pinned; return true; }
        }
        else if (category == "ideas")
        {
            for (auto& item : m_ideas)
                if (item.id == id) { item.pinned = !item.pinned; return true; }
        }
        else if (category == "questions")
        {
            for (auto& item : m_questions)
                if (item.id == id) { item.pinned = !item.pinned; return true; }
        }
        else if (category == "others" || category == "memories")
        {
            for (auto& item : m_others)
                if (item.id == id) { item.pinned = !item.pinned; return true; }
        }
        return false;
    }

    bool DataService::moveItem(const std::string& fromCategory, const std::string& toCategory, uint32_t id)
    {
        if (fromCategory == toCategory) return true;

        std::string title, content, comments;
        bool pinned = false;

        auto eraseFrom = [&](auto& vec) -> bool {
            for (auto it = vec.begin(); it != vec.end(); ++it)
                if (it->id == id) { vec.erase(it); return true; }
            return false;
        };

        if (fromCategory == "reminders")
        {
            for (auto it = m_reminders.begin(); it != m_reminders.end(); ++it)
                if (it->id == id)
                {
                    title = it->title; comments = it->comments; pinned = it->pinned;
                    m_reminders.erase(it); break;
                }
        }
        else if (fromCategory == "ideas")
        {
            for (auto it = m_ideas.begin(); it != m_ideas.end(); ++it)
                if (it->id == id)
                {
                    title = it->title; content = it->content;
                    comments = it->comments; pinned = it->pinned;
                    m_ideas.erase(it); break;
                }
        }
        else if (fromCategory == "questions")
        {
            for (auto it = m_questions.begin(); it != m_questions.end(); ++it)
                if (it->id == id)
                {
                    title = it->text; content = it->answer;
                    comments = it->comments; pinned = it->pinned;
                    m_questions.erase(it); break;
                }
        }
        else if (fromCategory == "others" || fromCategory == "memories")
        {
            for (auto it = m_others.begin(); it != m_others.end(); ++it)
                if (it->id == id)
                {
                    title = it->title; content = it->content;
                    comments = it->comments; pinned = it->pinned;
                    m_others.erase(it); break;
                }
        }

        if (title.empty()) return false;

        if (toCategory == "reminders")
        {
            Reminder item;
            item.id       = nextSequentialId(m_reminders);
            item.title    = title;
            item.dateTime = "2026-01-01 12:00";
            item.comments = comments;
            item.pinned   = pinned;
            m_reminders.insert(m_reminders.begin(), item);
            return true;
        }
        if (toCategory == "ideas")
        {
            Idea item;
            item.id       = nextSequentialId(m_ideas);
            item.title    = title;
            item.content  = content;
            item.comments = comments;
            item.pinned   = pinned;
            m_ideas.insert(m_ideas.begin(), item);
            return true;
        }
        if (toCategory == "questions")
        {
            Question item;
            item.id       = nextSequentialId(m_questions);
            item.text     = title;
            item.answer   = content;
            item.answered = !content.empty();
            item.comments = comments;
            item.pinned   = pinned;
            m_questions.insert(m_questions.begin(), item);
            return true;
        }
        if (toCategory == "others" || toCategory == "memories")
        {
            Memory item;
            item.id       = nextSequentialId(m_others);
            item.title    = title;
            item.content  = content;
            item.category = "note";
            item.pinned   = pinned;
            m_others.insert(m_others.begin(), item);
            return true;
        }
        return false;
    }

    uint32_t DataService::nextId(const std::vector<uint32_t>& ids)
    {
        uint32_t maxId = 0;
        for (uint32_t id : ids) if (id > maxId) maxId = id;
        return maxId + 1;
    }

} // namespace VOXA