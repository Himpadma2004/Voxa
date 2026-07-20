#include "JsonStorage.h"
#include "SpiffsMutex.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace
{
    void skipWS(const std::string& src, std::size_t& pos)
    {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos])))
            ++pos;
    }

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

    std::string parseValue(const std::string& src, std::size_t& pos)
    {
        skipWS(src, pos);
        if (pos >= src.size()) return {};

        if (src[pos] == '"') return parseString(src, pos);

        // Number or keyword
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
        // SPIFFS doesn't need folder creation
    }

    std::string JsonStorage::loadJson(const std::string& filename) const
    {
        SpiffsLock lock("JsonStorage::loadJson");
        String path = "/" + String(filename.c_str());
        if (!SPIFFS.exists(path)) return {};

        File f = SPIFFS.open(path, "r");
        if (!f) return {};

        String content = f.readString();
        f.close();
        return std::string(content.c_str());
    }

    bool JsonStorage::saveJson(const std::string& filename, const std::string& json)
    {
        SpiffsLock lock("JsonStorage::saveJson");
        String path = "/" + String(filename.c_str());
        File f = SPIFFS.open(path, "w");
        if (!f) return false;

        f.print(json.c_str());
        f.flush();
        f.close();
        return true;
    }

    bool JsonStorage::deleteFile(const std::string& filename)
    {
        SpiffsLock lock("JsonStorage::deleteFile");
        String path = "/" + String(filename.c_str());
        return SPIFFS.remove(path);
    }

    std::vector<std::map<std::string, std::string>>
        JsonStorage::parseObjectArray(const std::string& json)
    {
        std::vector<std::map<std::string, std::string>> result;
        if (json.empty()) return result;

        std::size_t pos = 0;
        skipWS(json, pos);

        if (pos >= json.size() || json[pos] != '[') return result;
        ++pos;

        while (true)
        {
            skipWS(json, pos);
            if (pos >= json.size() || json[pos] == ']') break;

            if (json[pos] != '{') { ++pos; continue; }
            ++pos;

            std::map<std::string, std::string> obj;
            while (true)
            {
                skipWS(json, pos);
                if (pos >= json.size() || json[pos] == '}') break;

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

        if (pos >= json.size() || json[pos] != '{') return result;
        ++pos;

        while (true)
        {
            skipWS(json, pos);
            if (pos >= json.size() || json[pos] == '}') break;

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
}
