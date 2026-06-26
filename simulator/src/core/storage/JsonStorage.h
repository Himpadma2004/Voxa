#pragma once

#include <string>
#include <vector>
#include <map>

namespace VOXA
{
    /// Lightweight file-based key-value / JSON blob storage.
    ///
    /// Each "file" is stored as a plain UTF-8 text file containing a JSON
    /// string.  No external JSON library is required — callers that need
    /// structured data use the helper utilities below.
    class JsonStorage
    {
    public:
        /// Construct storage rooted at the given directory path.
        /// The directory is created on first use if it does not exist.
        explicit JsonStorage(std::string storageDirectory);

        // ---------------------------------------------------------------
        // Raw file I/O
        // ---------------------------------------------------------------

        /// Read the entire contents of <storageDir>/<filename> as a string.
        /// Returns an empty string if the file does not exist or cannot be read.
        [[nodiscard]] std::string loadJson(const std::string& filename) const;

        /// Write <json> to <storageDir>/<filename>, overwriting any existing file.
        /// Returns true on success.
        bool saveJson(const std::string& filename, const std::string& json);

        /// Delete <storageDir>/<filename>.
        /// Returns true if the file was removed (or did not exist).
        bool deleteFile(const std::string& filename);

        // ---------------------------------------------------------------
        // Minimal JSON helpers (no external lib required)
        // ---------------------------------------------------------------

        /// Parse a JSON array of flat objects into a list of string-keyed maps.
        /// Only handles string and numeric leaf values (stored as strings).
        /// Example input:  [{"id":"1","title":"Call Sofia"}]
        [[nodiscard]] static std::vector<std::map<std::string, std::string>>
            parseObjectArray(const std::string& json);

        /// Parse a single flat JSON object into a string-keyed map.
        [[nodiscard]] static std::map<std::string, std::string>
            parseObject(const std::string& json);

        /// Serialise a list of flat string-keyed maps to a JSON array string.
        [[nodiscard]] static std::string
            serializeObjectArray(const std::vector<std::map<std::string, std::string>>& rows);

        /// Serialise a single flat string-keyed map to a JSON object string.
        [[nodiscard]] static std::string
            serializeObject(const std::map<std::string, std::string>& obj);

        /// Return the root storage directory path.
        [[nodiscard]] const std::string& storageDirectory() const;

    private:
        /// Ensure the storage directory exists (creates it if needed).
        void ensureDirectory() const;

        /// Build the full path for a storage file.
        [[nodiscard]] std::string fullPath(const std::string& filename) const;

        std::string m_directory;
    };
}
