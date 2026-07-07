#pragma once

#include <string>
#include <vector>
#include "../models/Memory.h"

namespace VOXA
{
    class MemoryStorage;

    /// High-level memory retrieval, filtering, search, and sorting logic.
    class MemoryService
    {
    public:
        explicit MemoryService(MemoryStorage* storage);

        [[nodiscard]] std::vector<Memory> getAll();
        [[nodiscard]] Memory getById(uint32_t id);
        Memory add(const Memory& memory);
        bool update(const Memory& memory);
        bool remove(uint32_t id);
        bool favorite(uint32_t id, bool isFavorite);

        [[nodiscard]] std::vector<Memory> search(const std::string& query, const std::string& sortBy = "newest");
        [[nodiscard]] std::vector<Memory> getRecent(int maxCount = 10);
        [[nodiscard]] std::vector<Memory> getByCategory(const std::string& category);

    private:
        MemoryStorage* m_storage { nullptr };
    };
}
