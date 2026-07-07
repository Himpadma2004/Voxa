#ifndef VOXA_JSONSTORAGE_H
#define VOXA_JSONSTORAGE_H

#include <string>
#include <vector>
#include <map>

namespace VOXA
{
    class JsonStorage
    {
    public:
        explicit JsonStorage(std::string storageDirectory);

        // Raw file I/O on SPIFFS
        [[nodiscard]] std::string loadJson(const std::string& filename) const;
        bool saveJson(const std::string& filename, const std::string& json);
        bool deleteFile(const std::string& filename);

        // Minimal JSON helpers
        [[nodiscard]] static std::vector<std::map<std::string, std::string>>
            parseObjectArray(const std::string& json);

        [[nodiscard]] static std::map<std::string, std::string>
            parseObject(const std::string& json);

        [[nodiscard]] static std::string
            serializeObjectArray(const std::vector<std::map<std::string, std::string>>& rows);

        [[nodiscard]] static std::string
            serializeObject(const std::map<std::string, std::string>& obj);

        [[nodiscard]] const std::string& storageDirectory() const;

    private:
        std::string m_directory;
    };
}

#endif // VOXA_JSONSTORAGE_H
