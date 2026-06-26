#include "SearchService.h"
#include "StorageService.h"

#include <algorithm>
#include <cctype>

namespace VOXA
{
    SearchService::SearchService(StorageService* storage)
        : m_storage(storage)
    {
    }

    bool SearchService::containsIgnoreCase(const std::string& haystack,
                                           const std::string& needle)
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

    std::vector<SearchResult> SearchService::search(const std::string& query) const
    {
        if (query.empty()) return {};

        std::vector<SearchResult> results;

        // Reminders
        for (const auto& r : m_storage->loadAllReminders())
        {
            if (containsIgnoreCase(r.title, query) ||
                containsIgnoreCase(r.dateTime, query))
            {
                results.push_back({ r.title, "reminder", r.dateTime, r.id });
            }
        }

        // Memories
        for (const auto& m : m_storage->loadAllMemories())
        {
            if (containsIgnoreCase(m.title, query) ||
                containsIgnoreCase(m.content, query))
            {
                results.push_back({ m.title, "memory", m.timestamp, m.id });
            }
        }

        // Ideas
        for (const auto& idea : m_storage->loadAllIdeas())
        {
            if (containsIgnoreCase(idea.title, query) ||
                containsIgnoreCase(idea.content, query))
            {
                results.push_back({ idea.title, "idea", idea.timestamp, idea.id });
            }
        }

        // Questions
        for (const auto& q : m_storage->loadAllQuestions())
        {
            if (containsIgnoreCase(q.text, query) ||
                containsIgnoreCase(q.answer, query))
            {
                results.push_back({ q.text, "question", q.timestamp, q.id });
            }
        }

        return results;
    }

    std::vector<SearchResult> SearchService::getAll() const
    {
        std::vector<SearchResult> results;

        for (const auto& r : m_storage->loadAllReminders())
            results.push_back({ r.title, "reminder", r.dateTime, r.id });
        for (const auto& m : m_storage->loadAllMemories())
            results.push_back({ m.title, "memory", m.timestamp, m.id });
        for (const auto& i : m_storage->loadAllIdeas())
            results.push_back({ i.title, "idea", i.timestamp, i.id });
        for (const auto& q : m_storage->loadAllQuestions())
            results.push_back({ q.text, "question", q.timestamp, q.id });

        return results;
    }

    std::vector<SearchResult> SearchService::getRecent(int maxCount) const
    {
        auto all = getAll();
        if (maxCount >= 0 && static_cast<int>(all.size()) > maxCount)
            all.resize(static_cast<std::size_t>(maxCount));
        return all;
    }
}
