#pragma once

#include <string>

namespace VOXA
{
    /// Result returned by every ApiClient call.
    struct ApiResult
    {
        bool success{false};
        int httpCode{0};
        std::string body;        ///< raw response body
        std::string text;        ///< parsed "text" field from JSON
        std::string error;       ///< human-readable error (empty on success)
        std::string contentType; ///< parsed Content-Type header
    };

    /// Result returned by AI database search queries.
    struct AiSearchResult
    {
        bool success{false};
        std::string query;
        std::string answer;
        std::string error;
    };

    /**
     * Independent HTTP client for all VOXA ↔ Python backend communication.
     */
    class ApiClient
    {
    public:
        ApiClient();

        void begin();

        // --- Configuration ---
        void setBaseUrl(const std::string &url);
        std::string getBaseUrl() const;

        // --- Health ---
        bool isReachable();
        bool isHealthy();

        // --- Voice Upload ---
        /// POST raw WAV bytes from a PSRAM/DRAM buffer to /api/voice/upload-raw
        ApiResult uploadVoiceFromBuffer(const uint8_t* buf, size_t size, const std::string& recordedAt = "");
        /// POST a WAV file that was previously staged on SPIFFS (legacy/retry path)
        ApiResult uploadVoice(const std::string& filePath);

        // --- AI Database Search ---
        /// Type Search (POST /api/search)
        AiSearchResult searchAi(const std::string &query);
        /// Audio Search (POST /api/search/audio-raw)
        AiSearchResult searchAiAudio(const std::string &filePath);

        // --- Generic helpers ---
        ApiResult get(const std::string &endpoint);
        ApiResult post(const std::string &endpoint, const std::string &jsonBody);

    private:
        std::string m_baseUrl{"http://192.168.0.148:8000"};
        bool m_lastHealthResult{false};
        uint32_t m_lastHealthCheckMs{0};

        void loadBaseUrl();
        void saveBaseUrl(const std::string &url);
        std::string discoverBackendIP();

        std::string parseTextField(const std::string &json);
        std::string parseQueryField(const std::string &json);
        std::string parseAnswerField(const std::string &json);
        bool parseBoolField(const std::string &json, const std::string &key);
    };

    extern ApiClient apiClient;
    extern std::string g_currentlyUploadingPath;
}