#pragma once

#include <string>
#include <vector>
#include "../models/Memory.h"

namespace VOXA
{
    class JsonStorage;

    /// Storage manager for memory.json database.
    /// Uses write-through caching to avoid redundant disk reads.
    class MemoryStorage
    {
    public:
        explicit MemoryStorage(JsonStorage* storage);

        [[nodiscard]] std::vector<Memory> loadAll();
        bool save(const std::vector<Memory>& memories);
        bool add(const Memory& memory);
        bool update(const Memory& memory);
        bool remove(uint32_t id);

    private:
        JsonStorage* m_storage { nullptr };
        std::vector<Memory> m_cache;
        bool m_loaded { false };

        void ensureLoaded();
        static uint32_t nextId(const std::vector<Memory>& list);
    };
}
