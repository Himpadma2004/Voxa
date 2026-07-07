#include "IdeaService.h"
#include "StorageService.h"

#include <algorithm>

namespace VOXA
{
    IdeaService::IdeaService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Idea> IdeaService::getAll()
    {
        auto ideas = m_storage->loadAllIdeas();
        std::sort(ideas.begin(), ideas.end(),
                  [](const Idea& a, const Idea& b) { return a.id < b.id; });
        return ideas;
    }

    Idea IdeaService::add(const std::string& title, const std::string& content, const std::string& timestamp)
    {
        Idea idea;
        idea.id = 0;
        idea.title = title;
        idea.content = content;
        idea.timestamp = timestamp;
        m_storage->saveIdea(idea);

        auto all = m_storage->loadAllIdeas();
        for (auto it = all.rbegin(); it != all.rend(); ++it)
            if (it->title == title && it->timestamp == timestamp)
                return *it;
        return idea;
    }

    bool IdeaService::remove(uint32_t id)
    {
        return m_storage->deleteIdea(id);
    }
}
