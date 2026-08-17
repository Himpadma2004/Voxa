#include "ApiClient.h"
#include "../storage/SpiffsMutex.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Arduino.h>
#include <algorithm>

// NOTE: SDCardService / SD.h removed — SD card adapter physically unplugged.
// uploadVoiceFromBuffer() is the primary upload path (PSRAM → HTTP).
// uploadVoice(filePath) is kept as a SPIFFS-based retry path only.

// ── Configuration ─────────────────────────────────────────────────────────────
#define API_BOUNDARY "VoxaBoundary9C4F2A1D"
#define API_CONNECT_MS 15000
#define API_READ_MS 30000

namespace
{
    bool parseBaseUrl(const std::string &baseUrl, String &host, int &port)
    {
        String urlStr = baseUrl.c_str();
        int schemeEnd = urlStr.indexOf("://");
        if (schemeEnd < 0)
            return false;

        String rest = urlStr.substring(schemeEnd + 3);
        int slashPos = rest.indexOf('/');
        String hostPort = (slashPos >= 0) ? rest.substring(0, slashPos) : rest;
        int colonPos = hostPort.indexOf(':');
        host = (colonPos >= 0) ? hostPort.substring(0, colonPos) : hostPort;
        port = (colonPos >= 0) ? hostPort.substring(colonPos + 1).toInt() : 80;
        return host.length() > 0;
    }

    String readLineWithTimeout(WiFiClient &client, uint32_t timeoutMs)
    {
        String line = "";
        uint32_t start = millis();
        while (millis() - start < timeoutMs)
        {
            if (client.available())
            {
                char c = client.read();
                if (c == '\n')
                    break;
                line += c;
            }
            else
            {
                if (!client.connected())
                    break;
                delay(1);
            }
        }
        return line;
    }

    String readBodyWithTimeout(WiFiClient &client, uint32_t timeoutMs)
    {
        String body = "";
        uint32_t start = millis();
        while (client.connected() || client.available())
        {
            if (client.available())
            {
                body += (char)client.read();
                start = millis();
            }
            else
            {
                if (millis() - start > timeoutMs)
                {
                    Serial.println("[ApiClient] Body read timeout");
                    break;
                }
                delay(1);
            }
        }
        return body;
    }
}

namespace VOXA
{
    // Global singleton instance (declared extern in header)
    ApiClient apiClient;
    std::string g_currentlyUploadingPath = "";

    // ── Constructor ───────────────────────────────────────────────────────────
    ApiClient::ApiClient() = default;

    void ApiClient::begin()
    {
        loadBaseUrl();
    }

    // ── NVS Credential Store ──────────────────────────────────────────────────
    void ApiClient::loadBaseUrl()
    {
        Preferences prefs;
        prefs.begin("voxa-api", true);
        String url = prefs.getString("url", "http://192.168.1.7:8000");
        prefs.end();
        std::string cleanedUrl = url.c_str();
        if (!cleanedUrl.empty() && cleanedUrl.back() == '/')
        {
            cleanedUrl.pop_back();
        }
        m_baseUrl = cleanedUrl;
        Serial.print("[ApiClient] Base URL: ");
        Serial.println(m_baseUrl.c_str());
    }

    void ApiClient::saveBaseUrl(const std::string &url)
    {
        std::string cleanedUrl = url;
        if (!cleanedUrl.empty() && cleanedUrl.back() == '/')
        {
            cleanedUrl.pop_back();
        }
        Preferences prefs;
        prefs.begin("voxa-api", false);
        prefs.putString("url", cleanedUrl.c_str());
        prefs.end();
        m_baseUrl = cleanedUrl;
    }

    void ApiClient::setBaseUrl(const std::string &url) { saveBaseUrl(url); }
    std::string ApiClient::getBaseUrl() const { return m_baseUrl; }

    // ── Health Check ──────────────────────────────────────────────────────────
    bool ApiClient::isReachable()
    {
        if (WiFi.status() != WL_CONNECTED)
            return false;

        HTTPClient http;
        http.setTimeout(2000);
        String url = String(m_baseUrl.c_str()) + "/";
        if (!http.begin(url)) return false;
        int code = http.GET();
        http.end();
        return (code > 0);
    }

    bool ApiClient::isHealthy()
    {
        // Cache health result for 5 seconds to avoid spamming the backend
        const uint32_t CACHE_MS = 5000;
        if (millis() - m_lastHealthCheckMs < CACHE_MS && m_lastHealthCheckMs > 0)
        {
            return m_lastHealthResult;
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            m_lastHealthCheckMs = millis();
            m_lastHealthResult = false;
            return false;
        }

        HTTPClient http;
        http.setTimeout(600); // Ultra-fast 600ms timeout for non-blocking health probe
        String url = String(m_baseUrl.c_str()) + "/";

        bool ok = false;
        if (http.begin(url))
        {
            int httpCode = http.GET();
            if (httpCode > 0)
            {
                ok = true;
            }
            http.end();
        }

        m_lastHealthCheckMs = millis();
        m_lastHealthResult = ok;
        return ok;
    }

    // ── JSON Helpers ──────────────────────────────────────────────────────────
    std::string ApiClient::parseTextField(const std::string &json)
    {
        const std::string key = "\"text\"";
        size_t kPos = json.find(key);
        if (kPos == std::string::npos)
            return "";
        size_t colon = json.find(':', kPos);
        if (colon == std::string::npos)
            return "";
        size_t q1 = json.find('"', colon + 1);
        if (q1 == std::string::npos)
            return "";
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos)
            return "";
        return json.substr(q1 + 1, q2 - q1 - 1);
    }

    bool ApiClient::parseBoolField(const std::string &json, const std::string &key)
    {
        const std::string token = "\"" + key + "\"";
        size_t kPos = json.find(token);
        if (kPos == std::string::npos)
            return false;
        size_t colon = json.find(':', kPos);
        if (colon == std::string::npos)
            return false;
        size_t val = json.find_first_not_of(" \t\r\n", colon + 1);
        if (val == std::string::npos)
            return false;
        return json.compare(val, 4, "true") == 0;
    }

    // ── Voice Upload: Buffer → Cloud (Primary Path) ──────────────────────────
    // Posts raw WAV bytes directly from PSRAM/DRAM to the backend.
    // No SPIFFS write needed — the audio lives in the PSRAM buffer until upload.
    ApiResult ApiClient::uploadVoiceFromBuffer(const uint8_t* buf, size_t size)
    {
        ApiResult result;

        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi connection";
            Serial.println("[UploadBuffer] Aborted — no Wi-Fi");
            return result;
        }
        if (!buf || size == 0)
        {
            result.error = "Empty audio buffer";
            Serial.println("[UploadBuffer] Aborted — buffer is null or empty");
            return result;
        }

        const String url = String(m_baseUrl.c_str()) + "/api/voice/upload-raw";
        Serial.printf("[UploadBuffer] Posting %u bytes -> %s\n", (unsigned)size, url.c_str());

        const int    MAX_ATTEMPTS       = 3;
        const uint32_t RETRY_DELAY_MS[] = {500, 1500, 3000};
        int httpCode = 0;

        for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt)
        {
            HTTPClient http;
            http.begin(url);
            http.setTimeout(60000);
            http.addHeader("Content-Type", "audio/wav");

            httpCode = http.POST(const_cast<uint8_t*>(buf), size);
            Serial.printf("[UploadBuffer] Attempt %d/%d — HTTP %d\n", attempt, MAX_ATTEMPTS, httpCode);
            result.httpCode = httpCode;

            if (httpCode == HTTP_CODE_OK || httpCode == 201)
            {
                const String body = http.getString();
                result.body    = body.c_str();
                result.success = true;
                result.text    = parseTextField(result.body);
                Serial.printf("[UploadBuffer] SUCCESS — audio_id: %s\n", result.text.c_str());
                http.end();
                break;
            }
            else if (httpCode < 0)
            {
                result.error = "Network error: " + std::string(http.errorToString(httpCode).c_str());
                Serial.printf("[UploadBuffer] Network error: %s (%d)\n",
                              http.errorToString(httpCode).c_str(), httpCode);
                http.end();
                if (attempt < MAX_ATTEMPTS)
                {
                    Serial.printf("[UploadBuffer] Retrying in %u ms...\n", RETRY_DELAY_MS[attempt - 1]);
                    delay(RETRY_DELAY_MS[attempt - 1]);
                    continue;
                }
            }
            else
            {
                result.error = "HTTP " + std::to_string(httpCode);
                result.body  = http.getString().c_str();
                Serial.printf("[UploadBuffer] Server error %d: %s\n", httpCode, result.body.c_str());
                http.end();
                break;
            }
        }
        return result;
    }

    // ── Voice Upload: SPIFFS → Cloud (Legacy / Retry Path) ───────────────────
    // Used only for recordings that were staged to SPIFFS in a previous session.
    // New recordings go through uploadVoiceFromBuffer() above.
    ApiResult ApiClient::uploadVoice(const std::string &filePath)
    {
        ApiResult result;

        // ── Pre-flight checks ──────────────────────────────────────────────
        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi connection";
            Serial.println("[Upload] Aborted — no Wi-Fi");
            return result;
        }

        SpiffsLock lock("ApiClient::uploadVoice");

        File file = SPIFFS.open(filePath.c_str(), "r");
        if (!file)
        {
            result.error = "File not found on SPIFFS";
            Serial.printf("[Upload] Cannot open: %s\n", filePath.c_str());
            return result;
        }

        const size_t fileSize = file.size();
        if (fileSize == 0)
        {
            file.close();
            result.error = "File is empty (0 bytes)";
            Serial.println("[Upload] Skipped — empty file");
            return result;
        }

        // ── Build URL ─────────────────────────────────────────────────────
        const String url = String(m_baseUrl.c_str()) + "/api/voice/upload-raw";

        Serial.println("=== UPLOAD START ===");
        Serial.printf("  File : %s\n", filePath.c_str());
        Serial.printf("  Size : %u bytes\n", fileSize);
        Serial.printf("  URL  : %s\n", url.c_str());
        Serial.println("====================");

        // ── Send via HTTPClient, with retry on transient network errors ────
        // A reset/refused/timeout on one attempt doesn't mean the IP is wrong
        // (isHealthy() already confirmed it) — it's usually a one-off WiFi/LAN
        // blip. Retry a few times with a short backoff before giving up, so a
        // single bad packet doesn't permanently fail the recording.
        const int MAX_ATTEMPTS = 3;
        const uint32_t RETRY_DELAY_MS[MAX_ATTEMPTS] = {500, 1500, 3000};

        int httpCode = 0;
        for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt)
        {
            file.seek(0); // rewind — previous attempt (if any) consumed the stream

            HTTPClient http;
            http.begin(url);
            http.setTimeout(60000); // 60 s — enough for any WAV
            http.addHeader("Content-Type", "audio/wav");

            // sendRequest("POST", stream*, size) streams the file in one shot.
            // HTTPClient manages the TCP socket, TCP_NODELAY, and response reading.
            httpCode = http.sendRequest("POST", &file, fileSize);

            Serial.printf("[Upload] Attempt %d/%d — HTTP response code: %d\n",
                          attempt, MAX_ATTEMPTS, httpCode);

            result.httpCode = httpCode;

            if (httpCode == HTTP_CODE_OK || httpCode == 201)
            {
                const String body = http.getString();
                result.body = body.c_str();
                result.success = true;
                result.text = parseTextField(result.body);
                Serial.printf("[Upload] SUCCESS — audio_id: %s\n", result.text.c_str());
                http.end();
                break; // done, no need to retry
            }
            else if (httpCode < 0)
            {
                // Negative codes are ESP-IDF error values (connection refused, reset, timeout…)
                result.error = "Network error: " + std::string(http.errorToString(httpCode).c_str());
                Serial.printf("[Upload] Network error: %s (%d)\n",
                              http.errorToString(httpCode).c_str(), httpCode);
                http.end();

                if (attempt < MAX_ATTEMPTS)
                {
                    Serial.printf("[Upload] Retrying in %u ms...\n", RETRY_DELAY_MS[attempt - 1]);
                    delay(RETRY_DELAY_MS[attempt - 1]);
                    continue;
                }
                // Out of attempts — leave the file queued on SPIFFS; the
                // background uploader will pick it up again on the next pass.
                // We deliberately do NOT trigger a full subnet rescan here — see
                // isHealthy() for the one place auto-discovery should run.
            }
            else
            {
                result.error = "HTTP " + std::to_string(httpCode);
                const String body = http.getString();
                result.body = body.c_str();
                Serial.printf("[Upload] Server error %d: %s\n", httpCode, body.c_str());
                http.end();
                break; // a real HTTP error (4xx/5xx) won't fix itself by retrying
            }
        }

        file.close();
        return result;
    }

    // ── Generic GET ───────────────────────────────────────────────────────────
    ApiResult ApiClient::get(const std::string &endpoint)
    {
        ApiResult result;
        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi";
            return result;
        }

        String url = String(m_baseUrl.c_str()) + endpoint.c_str();

        HTTPClient http;
        http.begin(url);
        http.setTimeout(10000); // 10 seconds

        const char* headerKeys[] = {"Content-Type"};
        http.collectHeaders(headerKeys, 1);

        int httpCode = http.GET();
        result.httpCode = httpCode;

        if (httpCode == HTTP_CODE_OK)
        {
            result.body = http.getString().c_str();
            result.contentType = http.header("Content-Type").c_str();
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error = "HTTP " + std::to_string(httpCode);
            if (httpCode < 0)
            {
                Serial.printf("[ApiClient] GET failed with network error: %d (%s)\n", 
                              httpCode, http.errorToString(httpCode).c_str());
            }
        }
        http.end();
        return result;
    }

    // ── Generic POST (JSON) ───────────────────────────────────────────────────
    ApiResult ApiClient::post(const std::string &endpoint, const std::string &jsonBody)
    {
        ApiResult result;
        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi";
            return result;
        }

        String url = String(m_baseUrl.c_str()) + endpoint.c_str();

        HTTPClient http;
        http.begin(url);
        http.setTimeout(10000); // 10 seconds
        http.addHeader("Content-Type", "application/json");

        const char* headerKeys[] = {"Content-Type"};
        http.collectHeaders(headerKeys, 1);

        int httpCode = http.POST(jsonBody.c_str());
        result.httpCode = httpCode;

        if (httpCode == HTTP_CODE_OK || httpCode == 201)
        {
            result.body = http.getString().c_str();
            result.contentType = http.header("Content-Type").c_str();
            result.success = true;
            result.text = parseTextField(result.body);
        }
        else
        {
            result.success = false;
            result.error = "HTTP " + std::to_string(httpCode);
            if (httpCode < 0)
            {
                Serial.printf("[ApiClient] POST failed with network error: %d (%s)\n", 
                              httpCode, http.errorToString(httpCode).c_str());
            }
        }
        http.end();
        return result;
    }

    std::string ApiClient::discoverBackendIP()
    {
        if (WiFi.status() != WL_CONNECTED)
            return "";

        IPAddress localIP = WiFi.localIP();
        IPAddress subnet = WiFi.subnetMask();

        // Standard Class C subnet (/24)
        if (subnet[0] == 255 && subnet[1] == 255 && subnet[2] == 255)
        {
            Serial.println("[ApiClient] Scanning local subnet for active backend on port 8000...");
            IPAddress targetIP = localIP;

            for (int i = 1; i < 255; ++i)
            {
                if (i == localIP[3])
                    continue; // Skip own IP

                targetIP[3] = i;
                WiFiClient testClient;
                testClient.setTimeout(100); // 100ms socket timeout for local network sweep

                // Fast connect sweep: local port 8000 closed/open handshake is very fast
                bool connected = testClient.connect(targetIP, 8000);
                if (connected)
                {
                    String discoveredUrl = "http://" + targetIP.toString() + ":8000";
                    testClient.stop(); // always release the socket, success or not
                    Serial.printf("[ApiClient] Auto-discovered backend at: %s\n", discoveredUrl.c_str());
                    return discoveredUrl.c_str();
                }
                testClient.stop(); // release the socket even after a failed/timed-out attempt
                delay(1);          // Yield to prevent watchdog triggers
            }
            Serial.println("[ApiClient] Subnet scan complete: no backend found on port 8000");
        }
        return "";
    }

    // ── AI Memory Search (Type Search) ────────────────────────────────────────
    AiSearchResult ApiClient::searchAi(const std::string &query)
    {
        AiSearchResult result;
        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi Connection";
            return result;
        }

        String url = String(m_baseUrl.c_str()) + "/api/search";
        HTTPClient http;
        http.begin(url);
        http.setTimeout(35000); // 35 seconds for LLM recall query
        http.addHeader("Content-Type", "application/json");

        String jsonBody = "{\"query\":\"" + String(query.c_str()) + "\"}";
        int httpCode = http.POST(jsonBody);

        if (httpCode == HTTP_CODE_OK || httpCode == 201)
        {
            String body = http.getString();
            result.success = true;
            result.query = query;
            result.answer = parseAnswerField(body.c_str());
            if (result.answer.empty())
            {
                result.answer = parseTextField(body.c_str());
            }
        }
        else
        {
            result.success = false;
            result.error = "HTTP Error " + std::to_string(httpCode);
        }
        http.end();
        return result;
    }

    // ── AI Memory Search (Audio Search) ───────────────────────────────────────
    AiSearchResult ApiClient::searchAiAudio(const std::string &filePath)
    {
        AiSearchResult result;
        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi Connection";
            return result;
        }

        uint8_t* audioBuf = nullptr;
        size_t fileSize = 0;

        {
            SpiffsLock lock("ApiClient::searchAiAudio");
            if (!SPIFFS.exists(filePath.c_str()))
            {
                result.error = "Audio file missing";
                return result;
            }

            File file = SPIFFS.open(filePath.c_str(), "r");
            if (!file)
            {
                result.error = "Failed to open WAV file";
                return result;
            }

            fileSize = file.size();
            if (fileSize < 44)
            {
                file.close();
                result.error = "Audio recording is empty";
                return result;
            }

            audioBuf = (uint8_t*)malloc(fileSize);
            if (!audioBuf)
            {
                file.close();
                result.error = "Memory allocation failed";
                return result;
            }

            file.read(audioBuf, fileSize);
            file.close();
            // SPIFFS file is now closed and SpiffsLock is released!
        }

        String url = String(m_baseUrl.c_str()) + "/api/search/audio-raw";
        HTTPClient http;
        http.begin(url);
        http.setTimeout(45000); // 45 seconds timeout for Whisper + LLM recall
        http.addHeader("Content-Type", "audio/wav");

        int httpCode = http.POST(audioBuf, fileSize);
        free(audioBuf);

        if (httpCode == HTTP_CODE_OK || httpCode == 201)
        {
            String body = http.getString();
            result.success = true;
            result.query = parseQueryField(body.c_str());
            result.answer = parseAnswerField(body.c_str());
            if (result.answer.empty())
            {
                result.answer = parseTextField(body.c_str());
            }
        }
        else
        {
            result.success = false;
            result.error = "Server Error (" + std::to_string(httpCode) + ")";
        }
        http.end();
        return result;
    }

    std::string ApiClient::parseQueryField(const std::string &json)
    {
        std::size_t pos = json.find("\"query\":");
        if (pos == std::string::npos)
            return "";

        std::size_t startQuote = json.find('"', pos + 8);
        if (startQuote == std::string::npos)
            return "";

        std::string val;
        bool escaped = false;
        for (std::size_t i = startQuote + 1; i < json.length(); ++i)
        {
            char c = json[i];
            if (escaped)
            {
                if (c == 'n') val += '\n';
                else if (c == 't') val += '\t';
                else val += c;
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                break;
            }
            else
            {
                val += c;
            }
        }
        return val;
    }

    std::string ApiClient::parseAnswerField(const std::string &json)
    {
        std::size_t pos = json.find("\"answer\":");
        if (pos == std::string::npos)
            return "";

        std::size_t startQuote = json.find('"', pos + 9);
        if (startQuote == std::string::npos)
            return "";

        std::string val;
        bool escaped = false;
        for (std::size_t i = startQuote + 1; i < json.length(); ++i)
        {
            char c = json[i];
            if (escaped)
            {
                if (c == 'n') val += '\n';
                else if (c == 't') val += '\t';
                else val += c;
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                break;
            }
            else
            {
                val += c;
            }
        }
        return val;
    }
}