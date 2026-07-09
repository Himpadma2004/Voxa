#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <qrcode.h>
#include "display/Display.h"
#include "touch/Touch.h"
#include "screens/BootScreen.h"
#include "screens/HomeScreen.h"
#include "screens/ReminderScreen.h"
#include "screens/IdeasScreen.h"
#include "screens/QuestionsScreen.h"
#include "screens/SearchScreen.h"
#include "screens/RecordScreen.h"
#include "screens/OthersScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/SyncStatusScreen.h"
#include "screens/DetailScreen.h"
#include "screens/Transition.h"
#include "services/TimeService.h"
#include "services/WiFiManager.h"
#include "storage/JsonStorage.h"
#include "storage/MemoryStorage.h"
#include "services/StorageService.h"
#include "services/ReminderService.h"
#include "services/IdeaService.h"
#include "services/QuestionService.h"
#include "services/RecordingService.h"
#include "services/SettingsService.h"
#include "services/MemoryService.h"
#include "services/SearchService.h"
#include "services/ApiClient.h"

using namespace VOXA;

// Global services instantiations
namespace VOXA
{
    JsonStorage storage("/spiffs");
    StorageService storageService(&storage);
    MemoryStorage memoryStorage(&storage);
    
    ReminderService reminderService(&storageService);
    IdeaService ideaService(&storageService);
    QuestionService questionService(&storageService);
    RecordingService recordingService(&storageService);
    SettingsService settingsService(&storageService);
    MemoryService memoryService(&memoryStorage);
    SearchService searchService(&storageService);
    TimeService timeService;
    WiFiManager wifiManager;
}

Touch touch;
BootScreen boot;
HomeScreen home;
ReminderScreen reminderScreen;
IdeasScreen ideasScreen;
QuestionsScreen questionsScreen;
SearchScreen searchScreen;
RecordScreen recordScreen;
OthersScreen othersScreen;
SettingsScreen settingsScreen;
SyncStatusScreen syncStatusScreen;
DetailScreen detailScreen;

ScreenId activeScreen = ScreenId::Home;

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("=================================");
  Serial.println("VOXA Firmware Starting...");
  Serial.printf("PSRAM Found: %s\n", psramFound() ? "YES" : "NO");
  Serial.printf("PSRAM Total Size: %d bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM Free Size: %d bytes\n", ESP.getFreePsram());
  Serial.printf("SRAM Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=================================");

  // Initialize hardware display and touch first
  Display::begin();
  touch.begin();
  Serial.println("Display Initialized");
  Serial.println("Touch Initialized");

  // Show boot screen immediately to give visual feedback
  boot.show();

  // Mount local SPIFFS filesystem
  if (!SPIFFS.begin(true))
  {
    Serial.println("[SPIFFS] Mount FAILED!");
  }
  else
  {
    Serial.println("[SPIFFS] Mounted successfully.");
  }

  // Initialize system/RTC clock (NTP will sync once WiFi connects)
  timeService.begin();

  // Initialize WiFiManager preferences and legacy cleanup
  wifiManager.begin();
  
  bool wifiConnected = false;
  if (wifiManager.hasSavedCredentials())
  {
      Serial.println("[WiFi] Saved credentials found. Connecting...");
      wifiManager.connect();
      
      // Draw "Connecting to Wi-Fi..." screen
      uint16_t w = Display::width();
      uint16_t h = Display::height();
      
      // Draw vertical gradient background
      for (int y = 0; y < h; ++y)
      {
          float t = (float)y / (h - 1);
          uint8_t r = (uint8_t)((1.0f - t) * 8 + t * 18);
          uint8_t g = (uint8_t)((1.0f - t) * 8 + t * 14);
          uint8_t b = (uint8_t)((1.0f - t) * 12 + t * 28);
          Display::lcd.drawFastHLine(0, y, w, Display::lcd.color565(r, g, b));
      }
      
      Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
      Display::lcd.setTextDatum(textdatum_t::middle_center);
      Display::lcd.setTextColor(Display::lcd.color565(124, 92, 255));
      Display::lcd.drawString("Connecting to Wi-Fi", w * 0.5f, h * 0.35f);
      
      std::string savedSsid = wifiManager.getSSID();
      Display::lcd.setFont(&fonts::FreeSans9pt7b);
      Display::lcd.setTextColor(TFT_WHITE);
      Display::lcd.drawString(savedSsid.c_str(), w * 0.5f, h * 0.55f);
      Display::lcd.drawString("Please wait...", w * 0.5f, h * 0.70f);
      
      // Wait up to 8 seconds for connection
      unsigned long startMs = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000)
      {
          delay(100);
      }
      
      if (WiFi.status() == WL_CONNECTED)
      {
          Serial.println("[WiFi] Connection successful!");
          Display::lcd.fillScreen(TFT_BLACK);
          
          // Draw success message
          for (int y = 0; y < h; ++y)
          {
              float t = (float)y / (h - 1);
              uint8_t r = (uint8_t)((1.0f - t) * 8 + t * 18);
              uint8_t g = (uint8_t)((1.0f - t) * 8 + t * 14);
              uint8_t b = (uint8_t)((1.0f - t) * 12 + t * 28);
              Display::lcd.drawFastHLine(0, y, w, Display::lcd.color565(r, g, b));
          }
          
          Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
          Display::lcd.setTextColor(TFT_GREEN);
          Display::lcd.drawString("Wi-Fi Connected!", w * 0.5f, h * 0.40f);
          
          Display::lcd.setFont(&fonts::FreeSans9pt7b);
          Display::lcd.setTextColor(TFT_WHITE);
          Display::lcd.drawString(wifiManager.getIPAddress().c_str(), w * 0.5f, h * 0.65f);
          delay(1500);
          
          wifiConnected = true;
      }
      else
      {
          Serial.println("[WiFi] Connection failed / timed out.");
      }
  }
  
  if (!wifiConnected)
  {
      Serial.println("[WiFi] Entering Setup Mode (Captive Portal)...");
      wifiManager.startPortal();
      
      bool skipSetup = false;
      bool skipPressed = false;
      
      auto drawSetupScreen = [](const char* status, bool showSkipPressed)
      {
          uint16_t w = Display::width();
          uint16_t h = Display::height();
          
          // Draw vertical gradient background
          for (int y = 0; y < h; ++y)
          {
              float t = (float)y / (h - 1);
              uint8_t r = (uint8_t)((1.0f - t) * 8 + t * 18);
              uint8_t g = (uint8_t)((1.0f - t) * 8 + t * 14);
              uint8_t b = (uint8_t)((1.0f - t) * 12 + t * 28);
              Display::lcd.drawFastHLine(0, y, w, Display::lcd.color565(r, g, b));
          }
          
          // Check softAP station count
          int stations = WiFi.softAPgetStationNum();
          
          const char* qrText = "WIFI:S:VOXA-Setup;T:WPA;P:12345678;;";
          const char* stepText = "1. Scan to Connect";
          
          if (stations > 0)
          {
              qrText = "http://192.168.4.1/";
              stepText = "2. Scan to Open Portal";
          }
          
          // Draw QR Code on the left side
          QRCode qrcode;
          uint8_t qrcodeBytes[qrcode_getBufferSize(4)];
          qrcode_initText(&qrcode, qrcodeBytes, 4, ECC_LOW, qrText);

          int scale = 3;
          int qrSize = qrcode.size * scale; // 33 * 3 = 99
          int qrX = 25;
          int qrY = 70;

          // Draw white background border for the QR code
          Display::lcd.fillRect(qrX - 6, qrY - 6, qrSize + 12, qrSize + 12, TFT_WHITE);

          for (uint8_t y = 0; y < qrcode.size; y++)
          {
              for (uint8_t x = 0; x < qrcode.size; x++)
              {
                  uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
                  Display::lcd.fillRect(qrX + x * scale, qrY + y * scale, scale, scale, color);
              }
          }
          
          // Draw title on the right
          Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
          Display::lcd.setTextDatum(textdatum_t::middle_center);
          Display::lcd.setTextColor(Display::lcd.color565(124, 92, 255));
          Display::lcd.drawString("VOXA Setup", w * 0.72f, 32);
          
          // Draw Instructions on the right
          Display::lcd.setFont(&fonts::FreeSans9pt7b);
          Display::lcd.setTextColor(TFT_WHITE);
          Display::lcd.drawString(stepText, w * 0.72f, 75);
          
          Display::lcd.setTextColor(Display::lcd.color565(166, 123, 250));
          if (stations == 0)
          {
              Display::lcd.drawString("SSID: VOXA-Setup", w * 0.72f, 105);
              Display::lcd.drawString("Pass: 12345678", w * 0.72f, 125);
          }
          else
          {
              Display::lcd.drawString("Connected to AP", w * 0.72f, 105);
              Display::lcd.drawString("IP: 192.168.4.1", w * 0.72f, 125);
          }
          
          // Draw Status
          Display::lcd.setTextColor(TFT_YELLOW);
          Display::lcd.drawString(status, w * 0.72f, 160);
          
          // Draw Skip Button (bottom right)
          uint16_t skipBg = showSkipPressed ? Display::lcd.color565(124, 92, 255) : Display::lcd.color565(48, 48, 60);
          uint16_t skipText = showSkipPressed ? TFT_BLACK : TFT_WHITE;
          
          int skipX = w * 0.72f - 50;
          int skipY = h - 45;
          Display::lcd.fillRoundRect(skipX, skipY, 100, 26, 4, skipBg);
          Display::lcd.drawRoundRect(skipX, skipY, 100, 26, 4, Display::lcd.color565(100, 100, 110));
          
          Display::lcd.setTextColor(skipText);
          Display::lcd.drawString("Skip Setup", w * 0.72f, skipY + 13);
      };

      while (!wifiManager.isConnected() && !skipSetup)
      {
          wifiManager.loopPortal();
          
          // Update Watch UI Status
          std::string statusText = "Waiting for user...";
          auto portalStatus = wifiManager.getPortalStatus();
          if (portalStatus == WiFiManager::PortalStatus::Connecting)
          {
              statusText = "Connecting...";
          }
          else if (portalStatus == WiFiManager::PortalStatus::Failed)
          {
              statusText = "Connect Failed!";
          }
          else if (portalStatus == WiFiManager::PortalStatus::Success)
          {
              statusText = "Connected!";
          }
          
          // Draw portal screen
          drawSetupScreen(statusText.c_str(), skipPressed);
          
          // Handle Skip button touch input
          uint16_t tx = 0, ty = 0;
          if (touch.getPoint(tx, ty))
          {
              // Skip button bounds (right half, bottom corner)
              if (tx >= 170 && tx <= 290 &&
                  ty >= 180 && ty <= 235)
              {
                  skipPressed = true;
              }
          }
          else
          {
              if (skipPressed)
              {
                  skipPressed = false;
                  skipSetup = true;
                  wifiManager.skipPortal();
                  wifiManager.stopPortal();
                  Serial.println("[WiFi] Setup Portal skipped by user.");
                  
                  // Show Skip feedback
                  uint16_t w = Display::width();
                  uint16_t h = Display::height();
                  Display::lcd.fillScreen(TFT_BLACK);
                  Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
                  Display::lcd.setTextColor(TFT_YELLOW);
                  Display::lcd.setTextDatum(textdatum_t::middle_center);
                  Display::lcd.drawString("Setup Skipped", w * 0.5f, h * 0.5f);
                  delay(1000);
              }
          }
          
          delay(50);
      }
      
      if (wifiManager.isConnected())
      {
          // Show Connected feedback
          uint16_t w = Display::width();
          uint16_t h = Display::height();
          Display::lcd.fillScreen(TFT_BLACK);
          Display::lcd.setFont(&fonts::FreeSansBold12pt7b);
          Display::lcd.setTextColor(TFT_GREEN);
          Display::lcd.setTextDatum(textdatum_t::middle_center);
          Display::lcd.drawString("Wi-Fi Connected!", w * 0.5f, h * 0.40f);
          
          Display::lcd.setFont(&fonts::FreeSans9pt7b);
          Display::lcd.setTextColor(TFT_WHITE);
          Display::lcd.drawString(wifiManager.getIPAddress().c_str(), w * 0.5f, h * 0.65f);
          delay(1500);
          
          wifiManager.stopPortal();
      }
  }

  Serial.println("Boot complete. Starting main screen loop...");
}

void loop()
{
  ScreenId nextScreen = activeScreen;

  switch (activeScreen)
  {
    case ScreenId::Home:
      nextScreen = home.show(touch);
      break;
    case ScreenId::Reminders:
      Serial.println("Opening Reminder Screen...");
      nextScreen = reminderScreen.show(touch);
      break;
    case ScreenId::Ideas:
      Serial.println("Opening Ideas Screen...");
      nextScreen = ideasScreen.show(touch);
      break;
    case ScreenId::Questions:
      Serial.println("Opening Questions Screen...");
      nextScreen = questionsScreen.show(touch);
      break;
    case ScreenId::Search:
      Serial.println("Opening Search Screen...");
      nextScreen = searchScreen.show(touch);
      break;
    case ScreenId::Record:
      Serial.println("Opening Record Screen...");
      nextScreen = recordScreen.show(touch);
      break;
    case ScreenId::Others:
      Serial.println("Opening Others Screen...");
      nextScreen = othersScreen.show(touch);
      break;
    case ScreenId::Settings:
      Serial.println("Opening Settings Screen...");
      nextScreen = settingsScreen.show(touch);
      break;
    case ScreenId::SyncStatus:
      Serial.println("Opening Sync Status Screen...");
      nextScreen = syncStatusScreen.show(touch);
      break;
    case ScreenId::Detail:
      Serial.println("Opening Detail Screen...");
      nextScreen = detailScreen.show(touch);
      break;
    default:
      nextScreen = ScreenId::Home;
      break;
  }

  if (nextScreen != activeScreen)
  {
    g_lastScreenId = activeScreen;
    activeScreen = nextScreen;
  }

  delay(10);
}
