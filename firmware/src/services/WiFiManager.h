#pragma once

#include <string>

namespace VOXA
{
    class WiFiManager
    {
    public:
        enum class PortalStatus
        {
            Idle,
            Connecting,
            Failed,
            Success
        };

        WiFiManager();
        ~WiFiManager();

        void begin();
        void connect();
        void disconnect();
        [[nodiscard]] bool isConnected() const;
        [[nodiscard]] std::string getSSID() const;
        [[nodiscard]] std::string getIPAddress() const;

        // Secure Credentials Management
        bool hasSavedCredentials();
        void getSavedCredentials(std::string& ssid, std::string& password);
        void saveCredentials(const std::string& ssid, const std::string& password);
        void clearCredentials();

        // Setup Portal Methods
        void startPortal();
        void loopPortal();
        void stopPortal();
        void skipPortal();

        [[nodiscard]] PortalStatus getPortalStatus() const;
        [[nodiscard]] std::string getPortalError() const;
        [[nodiscard]] bool isPortalSkipped() const;

    private:
        std::string m_ssid;
        std::string m_password;

        void loadCredentials();

        // Opaque pointers to avoid exposing web server headers to consumer files
        void* m_dnsServer { nullptr };
        void* m_webServer { nullptr };

        PortalStatus m_portalStatus { PortalStatus::Idle };
        std::string m_portalError;
        bool m_portalSkipped { false };
    };

    extern WiFiManager wifiManager;
}
