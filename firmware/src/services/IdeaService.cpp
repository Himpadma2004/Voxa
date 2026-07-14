#include "IdeaService.h"
#include "DataService.h"
#include "StorageService.h"

namespace VOXA
{
    IdeaService::IdeaService(StorageService* storage)
        : m_storage(storage)
    {
    }

    std::vector<Idea> IdeaService::getAll()
    {
        return dataService.getIdeas();
    }

    Idea IdeaService::add(const std::string& title, const std::string& content, const std::string& timestamp)
    {
        Idea idea;
        idea.id = 0;
        idea.title = title;
        idea.content = content;
        idea.timestamp = timestamp;
        m_storage->saveIdea(idea);
        dataService.addIdeaLocal(idea);
        return idea;
    }

    bool IdeaService::remove(uint32_t id)
    {
        dataService.removeIdeaLocal(id);
        return m_storage->deleteIdea(id);
    }
}
