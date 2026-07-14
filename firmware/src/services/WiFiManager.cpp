#include "WiFiManager.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include "ApiClient.h"


#define DNS_PTR ((DNSServer*)m_dnsServer)
#define WEB_PTR ((WebServer*)m_webServer)

namespace
{
    // Beautiful, responsive Dark Mode HTML for the Setup Page
    const char SETUP_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VOXA Wi-Fi Setup</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: #0f0f12;
            color: #ffffff;
            margin: 0;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            box-sizing: border-box;
        }
        .container {
            width: 100%;
            max-width: 400px;
            background: #1a1b23;
            padding: 30px;
            border-radius: 16px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
            border: 1px solid #2e2f3d;
            box-sizing: border-box;
        }
        h1 {
            font-size: 24px;
            text-align: center;
            margin-bottom: 24px;
            color: #7c5cff;
            font-weight: 700;
            letter-spacing: 1px;
        }
        p {
            font-size: 14px;
            color: #a0a0b0;
            text-align: center;
            margin-bottom: 24px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            font-size: 12px;
            font-weight: 600;
            margin-bottom: 8px;
            color: #a0a0b0;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        input, select {
            width: 100%;
            padding: 12px;
            background: #0f0f12;
            border: 1px solid #2e2f3d;
            border-radius: 8px;
            color: #ffffff;
            font-size: 16px;
            box-sizing: border-box;
            outline: none;
            transition: border-color 0.2s;
        }
        input:focus, select:focus {
            border-color: #7c5cff;
        }
        button {
            width: 100%;
            padding: 14px;
            background: #7c5cff;
            border: none;
            border-radius: 8px;
            color: #ffffff;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.2s, transform 0.1s;
            margin-top: 10px;
        }
        button:hover {
            background: #6a4be0;
        }
        button:active {
            transform: scale(0.98);
        }
        .network-list {
            max-height: 150px;
            overflow-y: auto;
            border: 1px solid #2e2f3d;
            border-radius: 8px;
            background: #0f0f12;
            margin-bottom: 20px;
        }
        .network-item {
            padding: 12px;
            border-bottom: 1px solid #2e2f3d;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 14px;
            transition: background 0.2s;
        }
        .network-item:last-child {
            border-bottom: none;
        }
        .network-item:hover {
            background: #1a1b23;
        }
        .signal-strength {
            color: #7c5cff;
            font-size: 12px;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>VOXA WI-FI</h1>
        <p>Select your network and enter password to connect VOXA.</p>
        <form action="/save" method="POST">
            <div class="form-group">
                <label>Available Networks</label>
                <div class="network-list" id="networks">
                    <div class="network-item">
                        <span>Scanning for networks...</span>
                    </div>
                </div>
            </div>
            <div class="form-group">
                <label for="ssid">SSID</label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter network name" required>
            </div>
            <div class="form-group">
                <label for="password">Password</label>
                <input type="password" id="password" name="password" placeholder="Enter Wi-Fi password">
            </div>
            <div class="form-group">
                <label for="api_url">Backend API URL</label>
                <input type="text" id="api_url" name="api_url" placeholder="http://192.168.0.148:8000" value="%API_URL%">
            </div>
            <button type="submit">Save & Connect</button>
        </form>
    </div>
    <script>
        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('password').focus();
        }

        function loadNetworks() {
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    const list = document.getElementById('networks');
                    list.innerHTML = '';
                    if (data.length === 0) {
                        list.innerHTML = '<div class="network-item"><span>No networks found</span></div>';
                        return;
                    }
                    data.forEach(net => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.onclick = () => selectNetwork(net.ssid);
                        item.innerHTML = `<span>${net.ssid}</span><span class="signal-strength">${net.rssi}%</span>`;
                        list.appendChild(item);
                    });
                })
                .catch(err => {
                    console.error('Error scanning networks:', err);
                });
        }
        
        // Scan immediately and refresh every 10s
        loadNetworks();
        setInterval(loadNetworks, 10000);
    </script>
</body>
</html>
)rawhtml";

    std::string tempSSID = "";
    std::string tempPassword = "";
}

namespace VOXA
{
    WiFiManager::WiFiManager()
    {
    }

    WiFiManager::~WiFiManager()
    {
        stopPortal();
    }

    void WiFiManager::begin()
    {
        // Delete legacy SPIFFS JSON to prevent conflicts and keep Preferences as source of truth
        if (SPIFFS.exists("/wifi.json"))
        {
            SPIFFS.remove("/wifi.json");
            Serial.println("[WiFiManager] Deleted legacy /wifi.json");
        }

        loadCredentials();
    }

    void WiFiManager::connect()
    {
        if (m_ssid.empty())
        {
            Serial.println("[WiFiManager] No credentials saved. Cannot connect.");
            return;
        }

        Serial.print("[WiFiManager] Connecting to: ");
        Serial.println(m_ssid.c_str());

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(m_ssid.c_str(), m_password.empty() ? nullptr : m_password.c_str());
    }

    void WiFiManager::disconnect()
    {
        Serial.println("[WiFiManager] Disconnecting Wi-Fi...");
        WiFi.disconnect(true, false);
    }

    bool WiFiManager::isConnected() const
    {
        return (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0, 0, 0, 0));
    }

    std::string WiFiManager::getSSID() const
    {
        if (isConnected())
        {
            return WiFi.SSID().c_str();
        }
        return m_ssid;
    }

    std::string WiFiManager::getIPAddress() const
    {
        if (isConnected())
        {
            return WiFi.localIP().toString().c_str();
        }
        return "0.0.0.0";
    }

    bool WiFiManager::hasSavedCredentials()
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", true);
        String ssid = prefs.getString("ssid", "");
        prefs.end();
        return !ssid.isEmpty();
    }

    void WiFiManager::getSavedCredentials(std::string& ssid, std::string& password)
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", true);
        ssid = prefs.getString("ssid", "").c_str();
        password = prefs.getString("password", "").c_str();
        prefs.end();
    }

    void WiFiManager::saveCredentials(const std::string& ssid, const std::string& password)
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", false);
        prefs.putString("ssid", ssid.c_str());
        prefs.putString("password", password.c_str());
        prefs.end();

        m_ssid = ssid;
        m_password = password;
        Serial.println("[WiFiManager] Saved new credentials to Preferences");
    }

    void WiFiManager::clearCredentials()
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", false);
        prefs.clear();
        prefs.end();

        m_ssid = "";
        m_password = "";
        Serial.println("[WiFiManager] Cleared credentials from Preferences");
    }

    bool WiFiManager::shouldForcePortal()
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", true);
        bool force = prefs.getBool("force_portal", false);
        prefs.end();
        return force;
    }

    void WiFiManager::clearForcePortal()
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", false);
        prefs.putBool("force_portal", false);
        prefs.end();
        Serial.println("[WiFiManager] Cleared force portal flag");
    }

    void WiFiManager::setForcePortal(bool force)
    {
        Preferences prefs;
        prefs.begin("voxa-wifi", false);
        prefs.putBool("force_portal", force);
        prefs.end();
        Serial.printf("[WiFiManager] Set force portal flag to: %s\n", force ? "true" : "false");
    }

    void WiFiManager::loadCredentials()
    {
        getSavedCredentials(m_ssid, m_password);
        Serial.print("[WiFiManager] Loaded credentials: ");
        Serial.println(m_ssid.empty() ? "None" : m_ssid.c_str());
    }

    void WiFiManager::startPortal()
    {
        Serial.println("[WiFiManager] Starting Setup Access Point...");
        
        m_portalSkipped = false;
        m_portalStatus = PortalStatus::Idle;
        m_portalError = "";

        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("VOXA-Setup", "12345678");

        IPAddress apIP(192, 168, 4, 1);
        
        // Start DNSServer directing all requests to the softAP IP
        DNSServer* dns = new DNSServer();
        dns->start(53, "*", apIP);
        m_dnsServer = dns;

        // Initialize WebServer on port 80
        WebServer* web = new WebServer(80);

        web->on("/", [web]() {
            String html = SETUP_HTML;
            Preferences prefs;
            prefs.begin("voxa-api", true);
            String currentUrl = prefs.getString("url", "http://192.168.0.148:8000");
            prefs.end();
            html.replace("%API_URL%", currentUrl);
            web->send(200, "text/html", html);
        });

        web->on("/scan", [web]() {
            int n = WiFi.scanNetworks();
            String json = "[";
            for (int i = 0; i < n; ++i)
            {
                if (i > 0) json += ",";
                int quality = constrain(2 * (WiFi.RSSI(i) + 100), 0, 100);
                json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(quality) + "}";
            }
            json += "]";
            web->send(200, "application/json", json);
        });

        web->on("/save", [this, web]() {
            String ssid = web->arg("ssid");
            String pass = web->arg("password");
            String apiUrl = web->arg("api_url");

            tempSSID = ssid.c_str();
            tempPassword = pass.c_str();

            if (!apiUrl.isEmpty())
            {
                Preferences prefs;
                prefs.begin("voxa-api", false);
                prefs.putString("url", apiUrl);
                prefs.end();
                // Also update runtime base URL
                VOXA::apiClient.setBaseUrl(apiUrl.c_str());
                Serial.printf("[WiFiManager] Portal saved API URL: %s\n", apiUrl.c_str());
            }

            String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Connecting...</title>";
            html += "<style>body{font-family:sans-serif;background:#0f0f12;color:#fff;text-align:center;padding:50px 20px;}h1{color:#7c5cff;}</style></head>";
            html += "<body><h1>Connecting...</h1><p>Attempting to connect to <b>" + ssid + "</b>.</p>";
            html += "<p>VOXA is validating the connection. Your phone might disconnect from VOXA-Setup.</p></body></html>";

            web->send(200, "text/html", html);
            m_portalStatus = PortalStatus::Connecting;
        });

        // Redirect captive portal checks unconditionally to setup page
        web->onNotFound([web]() {
            web->sendHeader("Location", "http://192.168.4.1/", true);
            web->send(302, "text/plain", "");
        });

        web->begin();
        m_webServer = web;

        Serial.println("[WiFiManager] Captive Portal Webserver started.");
    }

    void WiFiManager::loopPortal()
    {
        if (m_dnsServer) DNS_PTR->processNextRequest();
        if (m_webServer) WEB_PTR->handleClient();

        if (m_portalStatus == PortalStatus::Connecting)
        {
            Serial.print("[WiFiManager] Portal connection test SSID: ");
            Serial.println(tempSSID.c_str());

            WiFi.disconnect();
            WiFi.begin(tempSSID.c_str(), tempPassword.empty() ? nullptr : tempPassword.c_str());

            unsigned long startMs = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startMs < 10000)
            {
                delay(100);
                if (m_dnsServer) DNS_PTR->processNextRequest();
                if (m_webServer) WEB_PTR->handleClient();
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                m_portalStatus = PortalStatus::Success;
                saveCredentials(tempSSID, tempPassword);
            }
            else
            {
                m_portalStatus = PortalStatus::Failed;
                m_portalError = "Failed to connect to " + tempSSID + ". Check password.";
                Serial.println("[WiFiManager] Portal connection test FAILED.");
                
                // Keep the soft AP open for retry
                WiFi.mode(WIFI_AP_STA);
                WiFi.softAP("VOXA-Setup", "12345678");
            }
        }
    }

    void WiFiManager::stopPortal()
    {
        if (m_webServer)
        {
            WEB_PTR->stop();
            delete WEB_PTR;
            m_webServer = nullptr;
        }

        if (m_dnsServer)
        {
            DNS_PTR->stop();
            delete DNS_PTR;
            m_dnsServer = nullptr;
        }

        WiFi.softAPdisconnect(true);
        Serial.println("[WiFiManager] Setup Access Point stopped.");
    }

    void WiFiManager::skipPortal()
    {
        m_portalSkipped = true;
    }

    bool WiFiManager::isPortalSkipped() const
    {
        return m_portalSkipped;
    }

    WiFiManager::PortalStatus WiFiManager::getPortalStatus() const
    {
        return m_portalStatus;
    }

    std::string WiFiManager::getPortalError() const
    {
        return m_portalError;
    }
}
