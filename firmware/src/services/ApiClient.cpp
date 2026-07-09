#include "ApiClient.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <Arduino.h>
#include <algorithm>

// ── Configuration ─────────────────────────────────────────────────────────────
#define API_BOUNDARY        "VoxaBoundary9C4F2A1D"
#define API_CONNECT_MS      15000
#define API_READ_MS         30000

namespace VOXA
{
    // Global singleton instance (declared extern in header)
    ApiClient apiClient;

    // ── Constructor ───────────────────────────────────────────────────────────
    ApiClient::ApiClient()
    {
        loadBaseUrl();
    }

    // ── NVS Credential Store ──────────────────────────────────────────────────
    void ApiClient::loadBaseUrl()
    {
        Preferences prefs;
        prefs.begin("voxa-api", true);
        String url = prefs.getString("url", "http://192.168.1.100:8000");
        prefs.end();
        m_baseUrl = url.c_str();
        Serial.print("[ApiClient] Base URL: ");
        Serial.println(m_baseUrl.c_str());
    }

    void ApiClient::saveBaseUrl(const std::string& url)
    {
        Preferences prefs;
        prefs.begin("voxa-api", false);
        prefs.putString("url", url.c_str());
        prefs.end();
        m_baseUrl = url;
    }

    void ApiClient::setBaseUrl(const std::string& url) { saveBaseUrl(url); }
    std::string ApiClient::getBaseUrl() const          { return m_baseUrl; }

    // ── Health Check ──────────────────────────────────────────────────────────
    bool ApiClient::isReachable()
    {
        if (WiFi.status() != WL_CONNECTED) return false;

        // Parse host/port from base URL
        String urlStr = m_baseUrl.c_str();
        int schemeEnd = urlStr.indexOf("://");
        if (schemeEnd < 0) return false;
        String rest = urlStr.substring(schemeEnd + 3);
        int slashPos = rest.indexOf('/');
        String hostPort = (slashPos >= 0) ? rest.substring(0, slashPos) : rest;
        int colonPos = hostPort.indexOf(':');
        String host = (colonPos >= 0) ? hostPort.substring(0, colonPos) : hostPort;
        int    port = (colonPos >= 0) ? hostPort.substring(colonPos + 1).toInt() : 80;

        WiFiClient c;
        c.setTimeout(3);
        bool ok = c.connect(host.c_str(), port);
        c.stop();
        return ok;
    }

    // ── JSON Helpers ──────────────────────────────────────────────────────────
    std::string ApiClient::parseTextField(const std::string& json)
    {
        const std::string key = "\"text\"";
        size_t kPos = json.find(key);
        if (kPos == std::string::npos) return "";
        size_t colon = json.find(':', kPos);
        if (colon == std::string::npos) return "";
        size_t q1 = json.find('"', colon + 1);
        if (q1 == std::string::npos) return "";
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return json.substr(q1 + 1, q2 - q1 - 1);
    }

    bool ApiClient::parseBoolField(const std::string& json, const std::string& key)
    {
        const std::string token = "\"" + key + "\"";
        size_t kPos = json.find(token);
        if (kPos == std::string::npos) return false;
        size_t colon = json.find(':', kPos);
        if (colon == std::string::npos) return false;
        size_t val = json.find_first_not_of(" \t\r\n", colon + 1);
        if (val == std::string::npos) return false;
        return json.compare(val, 4, "true") == 0;
    }

    // ── Voice Upload ──────────────────────────────────────────────────────────
    ApiResult ApiClient::uploadVoice(const std::string& filePath)
    {
        ApiResult result;

        if (WiFi.status() != WL_CONNECTED)
        {
            result.error = "No Wi-Fi connection";
            Serial.println("[ApiClient] Upload failed: No Wi-Fi");
            return result;
        }

        File file = SPIFFS.open(filePath.c_str(), "r");
        if (!file)
        {
            result.error = "Recording file not found";
            Serial.printf("[ApiClient] Upload failed: cannot open %s\n", filePath.c_str());
            return result;
        }

        size_t fileSize = file.size();
        Serial.printf("[ApiClient] Uploading %s (%u bytes)...\n", filePath.c_str(), fileSize);

        // Multipart components
        String partHeader =
            "--" API_BOUNDARY "\r\n"
            "Content-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n"
            "Content-Type: audio/wav\r\n\r\n";
        String partFooter = "\r\n--" API_BOUNDARY "--\r\n";
        size_t totalLen   = partHeader.length() + fileSize + partFooter.length();

        // ── Parse URL → host, port, path ──────────────────────────────────
        String urlStr   = (m_baseUrl + "/api/voice/upload").c_str();
        String host;
        int    port     = 80;
        String path     = "/api/voice/upload";

        int schemeEnd = urlStr.indexOf("://");
        if (schemeEnd >= 0)
        {
            String rest     = urlStr.substring(schemeEnd + 3);
            int slashPos    = rest.indexOf('/');
            String hostPort = (slashPos >= 0) ? rest.substring(0, slashPos) : rest;
            int colonPos    = hostPort.indexOf(':');
            if (colonPos >= 0)
            {
                host = hostPort.substring(0, colonPos);
                port = hostPort.substring(colonPos + 1).toInt();
            }
            else
            {
                host = hostPort;
            }
        }

        // ── Stream upload via raw WiFiClient (memory-efficient, no heap spike) ──
        WiFiClient client;
        client.setTimeout(API_READ_MS / 1000);

        if (!client.connect(host.c_str(), port))
        {
            file.close();
            result.error = std::string("Cannot reach server at ") + host.c_str() + ":" + std::to_string(port);
            Serial.printf("[ApiClient] Cannot connect to %s:%d\n", host.c_str(), port);
            return result;
        }

        // HTTP request line + headers
        client.printf("POST %s HTTP/1.1\r\n",       path.c_str());
        client.printf("Host: %s:%d\r\n",            host.c_str(), port);
        client.printf("Content-Type: multipart/form-data; boundary=" API_BOUNDARY "\r\n");
        client.printf("Content-Length: %u\r\n",     totalLen);
        client.print("Connection: close\r\n\r\n");

        // Multipart header
        client.print(partHeader);

        // Stream WAV file in 512-byte chunks
        uint8_t buf[512];
        size_t  remaining = fileSize;
        while (remaining > 0)
        {
            size_t toRead = std::min(remaining, (size_t)512);
            size_t actual = file.read(buf, toRead);
            if (actual == 0) break;
            client.write(buf, actual);
            remaining -= actual;
        }
        file.close();

        // Multipart footer
        client.print(partFooter);

        // Wait for response
        uint32_t t0 = millis();
        while (!client.available() && (millis() - t0) < (uint32_t)API_READ_MS)
        {
            delay(10);
        }

        if (!client.available())
        {
            client.stop();
            result.error = "Upload timeout — no server response";
            Serial.println("[ApiClient] Timeout waiting for response");
            return result;
        }

        // Read status line
        String statusLine = client.readStringUntil('\n');
        int    spacePos   = statusLine.indexOf(' ');
        int    httpCode   = (spacePos >= 0)
                            ? statusLine.substring(spacePos + 1, spacePos + 4).toInt()
                            : 0;
        result.httpCode = httpCode;

        // Skip headers
        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            if (line == "\r" || line.isEmpty()) break;
        }

        // Read body
        String body;
        while (client.available()) body += (char)client.read();
        client.stop();

        result.body = body.c_str();

        if (httpCode == 200 || httpCode == 201)
        {
            result.success = parseBoolField(result.body, "success");
            result.text    = parseTextField(result.body);
            Serial.printf("[ApiClient] Response OK. Text: \"%s\"\n", result.text.c_str());
        }
        else
        {
            result.error = "Server returned HTTP " + std::to_string(httpCode);
            Serial.printf("[ApiClient] Server error: %d | %s\n", httpCode, result.body.c_str());
        }

        return result;
    }

    // ── Generic GET ───────────────────────────────────────────────────────────
    ApiResult ApiClient::get(const std::string& endpoint)
    {
        ApiResult result;
        if (WiFi.status() != WL_CONNECTED) { result.error = "No Wi-Fi"; return result; }

        // Build full URL
        String urlStr = (m_baseUrl + endpoint).c_str();
        String host; int port = 80; String path = endpoint.c_str();

        int schemeEnd = urlStr.indexOf("://");
        if (schemeEnd >= 0)
        {
            String rest  = urlStr.substring(schemeEnd + 3);
            int sl       = rest.indexOf('/');
            String hp    = (sl >= 0) ? rest.substring(0, sl) : rest;
            path         = (sl >= 0) ? rest.substring(sl) : "/";
            int col      = hp.indexOf(':');
            host = (col >= 0) ? hp.substring(0, col) : hp;
            port = (col >= 0) ? hp.substring(col + 1).toInt() : 80;
        }

        WiFiClient client;
        client.setTimeout(API_READ_MS / 1000);
        if (!client.connect(host.c_str(), port)) { result.error = "Cannot connect"; return result; }

        client.printf("GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
                      path.c_str(), host.c_str(), port);

        uint32_t t0 = millis();
        while (!client.available() && millis() - t0 < (uint32_t)API_READ_MS) delay(10);

        String statusLine = client.readStringUntil('\n');
        int sp = statusLine.indexOf(' ');
        result.httpCode = (sp >= 0) ? statusLine.substring(sp + 1, sp + 4).toInt() : 0;

        while (client.connected()) { String l = client.readStringUntil('\n'); if (l == "\r" || l.isEmpty()) break; }
        String body;
        while (client.available()) body += (char)client.read();
        client.stop();

        result.body = body.c_str();
        result.success = (result.httpCode == 200);
        if (!result.success) result.error = "HTTP " + std::to_string(result.httpCode);
        return result;
    }

    // ── Generic POST (JSON) ───────────────────────────────────────────────────
    ApiResult ApiClient::post(const std::string& endpoint, const std::string& jsonBody)
    {
        ApiResult result;
        if (WiFi.status() != WL_CONNECTED) { result.error = "No Wi-Fi"; return result; }

        String urlStr = (m_baseUrl + endpoint).c_str();
        String host; int port = 80; String path = endpoint.c_str();

        int schemeEnd = urlStr.indexOf("://");
        if (schemeEnd >= 0)
        {
            String rest = urlStr.substring(schemeEnd + 3);
            int sl      = rest.indexOf('/');
            String hp   = (sl >= 0) ? rest.substring(0, sl) : rest;
            path        = (sl >= 0) ? rest.substring(sl) : "/";
            int col     = hp.indexOf(':');
            host = (col >= 0) ? hp.substring(0, col) : hp;
            port = (col >= 0) ? hp.substring(col + 1).toInt() : 80;
        }

        WiFiClient client;
        client.setTimeout(API_READ_MS / 1000);
        if (!client.connect(host.c_str(), port)) { result.error = "Cannot connect"; return result; }

        client.printf("POST %s HTTP/1.1\r\n",        path.c_str());
        client.printf("Host: %s:%d\r\n",             host.c_str(), port);
        client.printf("Content-Type: application/json\r\n");
        client.printf("Content-Length: %u\r\n",      jsonBody.size());
        client.print("Connection: close\r\n\r\n");
        client.print(jsonBody.c_str());

        uint32_t t0 = millis();
        while (!client.available() && millis() - t0 < (uint32_t)API_READ_MS) delay(10);

        String statusLine = client.readStringUntil('\n');
        int sp = statusLine.indexOf(' ');
        result.httpCode = (sp >= 0) ? statusLine.substring(sp + 1, sp + 4).toInt() : 0;

        while (client.connected()) { String l = client.readStringUntil('\n'); if (l == "\r" || l.isEmpty()) break; }
        String body;
        while (client.available()) body += (char)client.read();
        client.stop();

        result.body    = body.c_str();
        result.success = (result.httpCode == 200 || result.httpCode == 201);
        if (!result.success) result.error = "HTTP " + std::to_string(result.httpCode);
        else
        {
            result.text = parseTextField(result.body);
        }
        return result;
    }
}
