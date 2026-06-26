#include "MemoryService.h"
#include "../storage/MemoryStorage.h"

#include <algorithm>
#include <cctype>

namespace
{
    bool containsIgnoreCase(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty()) return true;
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(),   needle.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
        return it != haystack.end();
    }
}

namespace VOXA
{
    MemoryService::MemoryService(MemoryStorage* storage)
        : m_storage(storage)
    {
    }

    std::vector<Memory> MemoryService::getAll()
    {
        return m_storage->loadAll();
    }

    Memory MemoryService::getById(uint32_t id)
    {
        auto list = m_storage->loadAll();
        for (const auto& m : list)
        {
            if (m.id == id) return m;
        }
        return {};
    }

    Memory MemoryService::add(const Memory& memory)
    {
        Memory m = memory;
        m_storage->add(m);
        // Reload list to find the newly assigned auto-increment id
        auto list = m_storage->loadAll();
        if (!list.empty())
        {
            return list.back();
        }
        return m;
    }

    bool MemoryService::update(const Memory& memory)
    {
        return m_storage->update(memory);
    }

    bool MemoryService::remove(uint32_t id)
    {
        return m_storage->remove(id);
    }

    bool MemoryService::favorite(uint32_t id, bool isFavorite)
    {
        auto list = m_storage->loadAll();
        for (auto& m : list)
        {
            if (m.id == id)
            {
                m.favorite = isFavorite;
                return m_storage->update(m);
            }
        }
        return false;
    }

    std::vector<Memory> MemoryService::search(const std::string& query, const std::string& sortBy)
    {
        auto list = m_storage->loadAll();
        std::vector<Memory> results;

        if (query.empty())
        {
            results = list;
        }
        else
        {
            for (const auto& m : list)
            {
                if (containsIgnoreCase(m.title, query) ||
                    containsIgnoreCase(m.content, query) ||
                    containsIgnoreCase(m.tags, query) ||
                    containsIgnoreCase(m.category, query))
                {
                    results.push_back(m);
                }
            }
        }

        // Apply Sorting
        std::sort(results.begin(), results.end(), [&sortBy](const Memory& a, const Memory& b) {
            if (sortBy == "newest")
            {
                return a.createdAt > b.createdAt;
            }
            else if (sortBy == "oldest")
            {
                return a.createdAt < b.createdAt;
            }
            else if (sortBy == "importance")
            {
                return a.importance > b.importance;
            }
            else if (sortBy == "favorites")
            {
                // place favorites first, tie-break by newest
                if (a.favorite != b.favorite)
                {
                    return a.favorite > b.favorite;
                }
                return a.createdAt > b.createdAt;
            }
            else if (sortBy == "alphabetical")
            {
                return a.title < b.title;
            }
            return a.id > b.id;
        });

        return results;
    }

    std::vector<Memory> MemoryService::getRecent(int maxCount)
    {
        auto list = getAll();
        std::sort(list.begin(), list.end(), [](const Memory& a, const Memory& b) {
            return a.createdAt > b.createdAt;
        });

        if (maxCount >= 0 && static_cast<int>(list.size()) > maxCount)
        {
            list.resize(static_cast<std::size_t>(maxCount));
        }
        return list;
    }

    std::vector<Memory> MemoryService::getByCategory(const std::string& category)
    {
        auto list = m_storage->loadAll();
        std::vector<Memory> results;
        for (const auto& m : list)
        {
            if (containsIgnoreCase(m.category, category))
            {
                results.push_back(m);
            }
        }
        return results;
    }
}
