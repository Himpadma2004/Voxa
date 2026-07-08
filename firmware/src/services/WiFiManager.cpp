#include "WiFiManager.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "../storage/JsonStorage.h"

namespace VOXA
{
    void WiFiManager::connect()
    {
        loadCredentials();

        Serial.print("[WiFiManager] Connecting to SSID: ");
        Serial.println(m_ssid.c_str());

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        
        // Start connection
        WiFi.begin(m_ssid.c_str(), m_password.empty() ? nullptr : m_password.c_str());
    }

    void WiFiManager::disconnect()
    {
        Serial.println("[WiFiManager] Disconnecting Wi-Fi...");
        WiFi.disconnect(true, false); // turn off radio, do not erase AP config
    }

    bool WiFiManager::isConnected() const
    {
        return WiFi.status() == WL_CONNECTED;
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

    void WiFiManager::loadCredentials()
    {
        if (!SPIFFS.exists("/wifi.json"))
        {
            saveDefaultCredentials();
            return;
        }

        File file = SPIFFS.open("/wifi.json", "r");
        if (!file)
        {
            m_ssid = "Wokwi-GUEST";
            m_password = "";
            return;
        }

        String content = file.readString();
        file.close();

        // Parse using global helper
        auto data = JsonStorage::parseObject(content.c_str());
        m_ssid = data["ssid"];
        m_password = data["password"];

        if (m_ssid.empty())
        {
            m_ssid = "Wokwi-GUEST";
        }
    }

    void WiFiManager::saveDefaultCredentials()
    {
        File file = SPIFFS.open("/wifi.json", "w");
        if (file)
        {
            file.print("{\n  \"ssid\": \"Wokwi-GUEST\",\n  \"password\": \"\"\n}");
            file.close();
            Serial.println("[WiFiManager] Created default /wifi.json");
        }
        m_ssid = "Wokwi-GUEST";
        m_password = "";
    }
}
