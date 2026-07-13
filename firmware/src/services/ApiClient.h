#pragma once

#include <string>

namespace VOXA
{
    /// Result returned by every ApiClient call.
    struct ApiResult
    {
        bool        success  { false };
        int         httpCode { 0 };
        std::string body;        ///< raw response body
        std::string text;        ///< parsed "text" field from JSON
        std::string error;       ///< human-readable error (empty on success)
    };

    /**
     * Independent HTTP client for all VOXA ↔ Python backend communication.
     *
     * Usage:
     *   apiClient.setBaseUrl("http://192.168.0.148:8000");
     *   ApiResult r = apiClient.uploadVoice("/voice_rec.wav");
     *
     * Future endpoints (no code changes needed, just call get/post):
     *   apiClient.get("/api/reminders");
     *   apiClient.post("/api/chat", "{\"message\":\"hello\"}");
     *   apiClient.get("/api/ota/check");
     */
    class ApiClient
    {
    public:
        ApiClient();

        // --- Configuration ---
        void        setBaseUrl(const std::string& url);
        std::string getBaseUrl() const;

        // --- Health ---
        bool        isReachable();

        // --- Voice Upload ---
        /// POST multipart/form-data to /api/voice/upload
        /// Returns ApiResult with transcribed text in result.text
        ApiResult   uploadVoice(const std::string& filePath);

        // --- Generic helpers (future: AI chat, sync, OTA, ...) ---
        ApiResult   get(const std::string& endpoint);
        ApiResult   post(const std::string& endpoint, const std::string& jsonBody);

    private:
        std::string m_baseUrl{"http://192.168.0.148:8000"};

        void        loadBaseUrl();
        void        saveBaseUrl(const std::string& url);
        std::string discoverBackendIP();

        std::string parseTextField(const std::string& json);
        bool        parseBoolField(const std::string& json, const std::string& key);
    };


    extern ApiClient apiClient;
    extern std::string g_currentlyUploadingPath;
}
