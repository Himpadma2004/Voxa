#include "JsonStorage.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#if defined(_WIN32)
#   include <direct.h>   // _mkdir
#   define VOXA_MKDIR(path) _mkdir(path)
#else
#   include <sys/types.h>
#   define VOXA_MKDIR(path) mkdir(path, 0755)
#endif

namespace
{
    // -----------------------------------------------------------------------
    // Very small JSON tokeniser / parser helpers
    // -----------------------------------------------------------------------

    /// Skip whitespace characters in src starting at pos.
    void skipWS(const std::string& src, std::size_t& pos)
    {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
    }

    /// Parse a JSON string value (including surrounding quotes).
    /// pos should point at the opening '"'. Returns the unescaped value.
    std::string parseString(const std::string& src, std::size_t& pos)
    {
        if (pos >= src.size() || src[pos] != '"') return {};
        ++pos; // skip opening quote
        std::string result;
        while (pos < src.size() && src[pos] != '"')
        {
            if (src[pos] == '\\' && pos + 1 < src.size())
            {
                ++pos;
                switch (src[pos])
                {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += src[pos]; break;
                }
            }
            else
            {
                result += src[pos];
            }
            ++pos;
        }
        if (pos < src.size()) ++pos; // skip closing quote
        return result;
    }

    /// Parse a JSON value (string, number, or boolean) and return it as a string.
    std::string parseValue(const std::string& src, std::size_t& pos)
    {
        skipWS(src, pos);
        if (pos >= src.size()) return {};

        if (src[pos] == '"') return parseString(src, pos);

        // Number or keyword (true/false/null)
        std::size_t start = pos;
        while (pos < src.size() && src[pos] != ',' && src[pos] != '}' &&
               src[pos] != ']' && !std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
        return src.substr(start, pos - start);
    }
}

namespace VOXA
{
    JsonStorage::JsonStorage(std::string storageDirectory)
        : m_directory(std::move(storageDirectory))
    {
        ensureDirectory();
    }

    // -----------------------------------------------------------------------
    // File I/O
    // -----------------------------------------------------------------------

    std::string JsonStorage::loadJson(const std::string& filename) const
    {
        const std::string path = fullPath(filename);
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs.is_open()) return {};

        std::ostringstream oss;
        oss << ifs.rdbuf();
        return oss.str();
    }

    bool JsonStorage::saveJson(const std::string& filename, const std::string& json)
    {
        ensureDirectory();
        const std::string path = fullPath(filename);
        std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) return false;
        ofs << json;
        return ofs.good();
    }

    bool JsonStorage::deleteFile(const std::string& filename)
    {
        const std::string path = fullPath(filename);
        return std::remove(path.c_str()) == 0;
    }

    // -----------------------------------------------------------------------
    // Static JSON helpers
    // -----------------------------------------------------------------------

    std::vector<std::map<std::string, std::string>>
        JsonStorage::parseObjectArray(const std::string& json)
    {
        std::vector<std::map<std::string, std::string>> result;
        if (json.empty()) return result;

        std::size_t pos = 0;
        skipWS(json, pos);

        // Expect opening '['
        if (pos >= json.size() || json[pos] != '[') return result;
        ++pos;

        while (true)
        {
            skipWS(json, pos);
            if (pos >= json.size() || json[pos] == ']') break;

            // Expect '{'
            if (json[pos] != '{') { ++pos; continue; }
            ++pos;

            std::map<std::string, std::string> obj;
            while (true)
            {
                skipWS(json, pos);
                if (pos >= json.size() || json[pos] == '}') break;

                // Key
                if (json[pos] != '"') { ++pos; continue; }
                std::string key = parseString(json, pos);

                skipWS(json, pos);
                if (pos < json.size() && json[pos] == ':') ++pos;

                skipWS(json, pos);
                std::string value = parseValue(json, pos);
                obj[key] = value;

                skipWS(json, pos);
                if (pos < json.size() && json[pos] == ',') ++pos;
            }
            if (pos < json.size() && json[pos] == '}') ++pos;
            result.push_back(std::move(obj));

            skipWS(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
        }

        return result;
    }

    std::string JsonStorage::serializeObjectArray(
        const std::vector<std::map<std::string, std::string>>& rows)
    {
        std::ostringstream oss;
        oss << "[\n";
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            oss << "  {";
            const auto& row = rows[r];
            bool first = true;
            for (const auto& [key, val] : row)
            {
                if (!first) oss << ", ";
                first = false;

                oss << '"';
                for (char c : key)
                {
                    if (c == '"')      oss << "\\\"";
                    else if (c == '\\') oss << "\\\\";
                    else                oss << c;
                }
                oss << "\": \"";
                for (char c : val)
                {
                    if (c == '"')      oss << "\\\"";
                    else if (c == '\\') oss << "\\\\";
                    else if (c == '\n') oss << "\\n";
                    else if (c == '\r') oss << "\\r";
                    else if (c == '\t') oss << "\\t";
                    else                oss << c;
                }
                oss << '"';
            }
            oss << '}';
            if (r + 1 < rows.size()) oss << ',';
            oss << '\n';
        }
        oss << ']';
        return oss.str();
    }

    std::map<std::string, std::string>
        JsonStorage::parseObject(const std::string& json)
    {
        std::map<std::string, std::string> result;
        if (json.empty()) return result;

        std::size_t pos = 0;
        skipWS(json, pos);

        // Expect opening '{'
        if (pos >= json.size() || json[pos] != '{') return result;
        ++pos;

        while (true)
        {
            skipWS(json, pos);
            if (pos >= json.size() || json[pos] == '}') break;

            // Key
            if (json[pos] != '"') { ++pos; continue; }
            std::string key = parseString(json, pos);

            skipWS(json, pos);
            if (pos < json.size() && json[pos] == ':') ++pos;

            skipWS(json, pos);
            std::string value = parseValue(json, pos);
            result[key] = value;

            skipWS(json, pos);
            if (pos < json.size() && json[pos] == ',') ++pos;
        }
        return result;
    }

    std::string
        JsonStorage::serializeObject(const std::map<std::string, std::string>& obj)
    {
        std::ostringstream oss;
        oss << "{\n";
        bool first = true;
        for (const auto& [key, val] : obj)
        {
            if (!first) oss << ",\n";
            first = false;

            oss << "  \"" << key << "\": \"";
            for (char c : val)
            {
                if (c == '"')      oss << "\\\"";
                else if (c == '\\') oss << "\\\\";
                else if (c == '\n') oss << "\\n";
                else if (c == '\r') oss << "\\r";
                else if (c == '\t') oss << "\\t";
                else                oss << c;
            }
            oss << "\"";
        }
        oss << "\n}";
        return oss.str();
    }

    const std::string& JsonStorage::storageDirectory() const
    {
        return m_directory;
    }

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    void JsonStorage::ensureDirectory() const
    {
        if (m_directory.empty()) return;
        VOXA_MKDIR(m_directory.c_str());
    }

    std::string JsonStorage::fullPath(const std::string& filename) const
    {
        if (m_directory.empty()) return filename;
        return m_directory + "/" + filename;
    }
}
