#pragma once

#include <string>
#include <vector>

namespace VOXA
{
    class StorageService;

    /// A single result returned by SearchService::search().
    struct SearchResult
    {
        std::string title;
        std::string category;   ///< "reminder" | "memory" | "idea" | "question"
        std::string timestamp;
        uint32_t    sourceId { 0 };
    };

    /// Full-text search across all VOXA data categories.
    ///
    /// Screens (especially SearchScreen) should call this service instead of
    /// holding their own static data arrays.
    class SearchService
    {
    public:
        /// @param storage  Non-owning pointer. Must outlive this service.
        explicit SearchService(StorageService* storage);

        /// Search all categories for entries whose title or content contains
        /// the query string (case-insensitive).
        /// Returns an empty vector if the query is empty or blank.
        [[nodiscard]] std::vector<SearchResult> search(const std::string& query) const;

        /// Returns all entries across all categories (for displaying recent items).
        [[nodiscard]] std::vector<SearchResult> getAll() const;

        /// Returns the N most recently added entries.
        [[nodiscard]] std::vector<SearchResult> getRecent(int maxCount = 10) const;

    private:
        StorageService* m_storage { nullptr };

        static bool containsIgnoreCase(const std::string& haystack,
                                       const std::string& needle);
    };
}
