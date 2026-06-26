#include "MemoryStorage.h"
#include "JsonStorage.h"

#include <algorithm>
#include <map>
#include <iostream>

namespace VOXA
{
    namespace
    {
        const char* kMemoryFile = "memory.json";

        const char* kDefaultMemories = R"([
  {
    "id": "1",
    "title": "YouTube Integration Idea",
    "content": "Discuss integration of YouTube API with Voxa workspace.",
    "category": "idea",
    "tags": "api,youtube,voxa",
    "createdAt": "2026-06-26 10:00",
    "updatedAt": "2026-06-26 10:00",
    "importance": "4",
    "favorite": "true",
    "source": "Idea",
    "timestamp": "2 days ago",
    "durationSeconds": "0",
    "comments": "Need key credentials"
  },
  {
    "id": "2",
    "title": "Emily Birthday",
    "content": "Reminder to buy a gift and plan a surprise party for Emily.",
    "category": "reminder",
    "tags": "family,birthday,party",
    "createdAt": "2026-06-25 15:30",
    "updatedAt": "2026-06-25 15:30",
    "importance": "5",
    "favorite": "false",
    "source": "Reminder",
    "timestamp": "3 days ago",
    "durationSeconds": "0",
    "comments": ""
  }
])";
    }

    MemoryStorage::MemoryStorage(JsonStorage* storage)
        : m_storage(storage)
    {
    }

    void MemoryStorage::ensureLoaded()
    {
        if (m_loaded) return;

        std::string rawJson = m_storage->loadJson(kMemoryFile);

        // Seeding defaults on first run if file is missing or empty
        if (rawJson.empty())
        {
            m_storage->saveJson(kMemoryFile, kDefaultMemories);
            rawJson = kDefaultMemories;
        }

        std::vector<std::map<std::string, std::string>> rows;
        try
        {
            rows = JsonStorage::parseObjectArray(rawJson);
        }
        catch (...)
        {
            // Gracefully handle corrupted JSON
            std::cerr << "[MemoryStorage] WARNING: Corrupted JSON detected. Reverting to empty." << std::endl;
        }

        m_cache.clear();
        m_cache.reserve(rows.size());

        for (const auto& row : rows)
        {
            Memory m;
            auto get = [&](const char* k, const std::string& def = "") -> std::string {
                auto it = row.find(k);
                return it != row.end() ? it->second : def;
            };

            try { m.id = static_cast<uint32_t>(std::stoul(get("id"))); } catch(...) {}
            m.title           = get("title");
            m.content         = get("content");
            m.category        = get("category", "note");
            m.tags            = get("tags");
            m.createdAt       = get("createdAt");
            m.updatedAt       = get("updatedAt");
            try { m.importance = static_cast<uint32_t>(std::stoul(get("importance", "1"))); } catch(...) {}
            m.favorite        = (get("favorite") == "true");
            m.source          = get("source", "Manual");
            m.timestamp       = get("timestamp");
            try { m.durationSeconds = static_cast<uint32_t>(std::stoul(get("durationSeconds", "0"))); } catch(...) {}
            m.comments        = get("comments");

            if (m.isValid())
            {
                m_cache.push_back(std::move(m));
            }
        }

        m_loaded = true;
    }

    std::vector<Memory> MemoryStorage::loadAll()
    {
        ensureLoaded();
        return m_cache;
    }

    bool MemoryStorage::save(const std::vector<Memory>& memories)
    {
        m_cache = memories;
        m_loaded = true;

        std::vector<std::map<std::string, std::string>> rows;
        rows.reserve(memories.size());

        for (const auto& m : memories)
        {
            std::map<std::string, std::string> row;
            row["id"]              = std::to_string(m.id);
            row["title"]           = m.title;
            row["content"]         = m.content;
            row["category"]        = m.category;
            row["tags"]            = m.tags;
            row["createdAt"]       = m.createdAt;
            row["updatedAt"]       = m.updatedAt;
            row["importance"]      = std::to_string(m.importance);
            row["favorite"]        = m.favorite ? "true" : "false";
            row["source"]          = m.source;
            row["timestamp"]       = m.timestamp;
            row["durationSeconds"] = std::to_string(m.durationSeconds);
            row["comments"]        = m.comments;

            rows.push_back(std::move(row));
        }

        return m_storage->saveJson(kMemoryFile, JsonStorage::serializeObjectArray(rows));
    }

    bool MemoryStorage::add(const Memory& memory)
    {
        ensureLoaded();
        Memory m = memory;
        if (m.id == 0)
        {
            m.id = nextId(m_cache);
        }

        m_cache.push_back(m);
        return save(m_cache);
    }

    bool MemoryStorage::update(const Memory& memory)
    {
        ensureLoaded();
        bool found = false;
        for (auto& m : m_cache)
        {
            if (m.id == memory.id)
            {
                m = memory;
                found = true;
                break;
            }
        }
        if (!found) return false;
        return save(m_cache);
    }

    bool MemoryStorage::remove(uint32_t id)
    {
        ensureLoaded();
        auto it = std::remove_if(m_cache.begin(), m_cache.end(),
            [id](const Memory& m) { return m.id == id; });
        if (it == m_cache.end()) return false;
        m_cache.erase(it, m_cache.end());
        return save(m_cache);
    }

    uint32_t MemoryStorage::nextId(const std::vector<Memory>& list)
    {
        uint32_t maxId = 0;
        for (const auto& m : list)
        {
            if (m.id > maxId) maxId = m.id;
        }
        return maxId + 1;
    }
}
