#pragma once

#include <string>

namespace VOXA
{
    class WiFiManager
    {
    public:
        WiFiManager() = default;

        void connect();
        void disconnect();
        [[nodiscard]] bool isConnected() const;
        [[nodiscard]] std::string getSSID() const;
        [[nodiscard]] std::string getIPAddress() const;

    private:
        std::string m_ssid;
        std::string m_password;

        void loadCredentials();
        void saveDefaultCredentials();
    };

    extern WiFiManager wifiManager;
}
