#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../models/Idea.h"

namespace VOXA
{
    class StorageService;

    /// High-level business logic for ideas.
    class IdeaService
    {
    public:
        explicit IdeaService(StorageService* storage);

        [[nodiscard]] std::vector<Idea> getAll();
        Idea add(const std::string& title, const std::string& content, const std::string& timestamp);
        bool remove(uint32_t id);

    private:
        StorageService* m_storage { nullptr };
    };
}
