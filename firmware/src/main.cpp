#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <qrcode.h>
#include <esp_system.h>          // esp_reset_reason()
#include <esp_task_wdt.h>        // task watchdog control
#include "soc/soc.h"             // WRITE_PERI_REG macro
#include "soc/rtc_cntl_reg.h"   // RTC_CNTL_BROWN_OUT_REG — brownout disable register
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
#include "screens/RecordingsLibraryScreen.h"
#include "screens/AudioPlayerScreen.h"
#include "screens/WiFiSettingsScreen.h"
#include "screens/BluetoothSettingsScreen.h"
#include "screens/TextInputScreen.h"

#include "screens/TasksScreen.h"
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
#include "services/DataService.h"
#include "services/ApiClient.h"
#include "storage/SpiffsMutex.h"
#include "storage/StorageManager.h"  // SPIFFS-only mode
#include "reminders/ReminderManager.h"
#include "services/BatteryManager.h"
#include "audio/AudioManager.h"
#include "services/ButtonService.h"
#include "services/PowerManager.h"
// NOTE: SDCardService.h removed — SD card adapter physically unplugged.

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
RecordingsLibraryScreen recordingsLibraryScreen;
AudioPlayerScreen audioPlayerScreen;
WiFiSettingsScreen wifiSettingsScreen;
BluetoothSettingsScreen bluetoothSettingsScreen;
TextInputScreen textInputScreen;

TasksScreen tasksScreen;

ScreenId activeScreen = ScreenId::Home;

namespace
{
  void waitForWiFiConnection(const char *tag)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      return;
    }

    Serial.printf("[%s] Waiting for Wi-Fi connection...\n", tag);
    uint32_t waitStart = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      esp_task_wdt_reset(); // feed the watchdog so WDT doesn't trigger during long wifi wait
      vTaskDelay(pdMS_TO_TICKS(500));

      // Give up waiting after 30 seconds — don't block forever on battery
      if (millis() - waitStart > 30000)
      {
        Serial.printf("[%s] Wi-Fi wait timeout (30s). Continuing offline.\n", tag);
        return;
      }
    }
    Serial.printf("[%s] Wi-Fi connected.\n", tag);
  }

  void backgroundDataSyncTask(void * /*param*/)
  {
    uint32_t lastSyncMs = 0;
    bool lastConnected = false;

    while (true)
    {
      if (microphoneService.isBusy())
      {
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }

      waitForWiFiConnection("DataSync");
      bool connected = true;
      uint32_t now = millis();

      if (connected)
      {
        if (!VOXA::apiClient.isHealthy())
        {
          Serial.println("[DataSync] Backend not responding. Will retry later.");
          lastConnected = false;
          vTaskDelay(pdMS_TO_TICKS(15000));
          continue;
        }

        // Sync on reconnection OR if 10 seconds have elapsed since the last sync
        if (!lastConnected || (now - lastSyncMs >= 10000) || lastSyncMs == 0)
        {
          Serial.println("[DataSync] Syncing backend data in background...");
          VOXA::dataService.syncAll();
          lastSyncMs = millis();
        }
      }

      lastConnected = connected;
      vTaskDelay(pdMS_TO_TICKS(3000)); // Check connectivity state every 3 seconds
    }
  }

  void backgroundUploadTask(void* /*param*/)
  {
    // This task is now a lightweight retry monitor.
    // Primary recording upload happens synchronously in MicrophoneService::stopRecording()
    // (PSRAM buffer → HTTP POST). This task only handles recordings marked "Pending"
    // that were left over if a previous upload failed.
    while (true)
    {
      vTaskDelay(pdMS_TO_TICKS(5000)); // check every 5 seconds

      if (microphoneService.isBusy())
        continue;

      waitForWiFiConnection("BackgroundUpload");

      if (!apiClient.isHealthy())
      {
        Serial.println("[BackgroundUpload] Backend not responding. Will retry later.");
        continue;
      }

      // Sync from cloud to keep in-RAM data fresh
      VOXA::dataService.syncAll();

      // Check for any recordings still marked Pending (upload failed last time)
      auto recordings = recordingService.getAll();
      bool anySynced = false;
      for (auto& rec : recordings)
      {
        if (microphoneService.isBusy()) break;

        if (rec.timestamp == "Pending")
        {
          Serial.printf("[BackgroundUpload] Found Pending record: ID=%u title=%s\n",
                        rec.id, rec.title.c_str());
          // Mark as Failed so it doesn't retry forever if there's no local file
          rec.timestamp = "Failed";
          recordingService.update(rec);
          Serial.println("[BackgroundUpload] Marked stale Pending record as Failed.");
          anySynced = true;
        }
      }

      if (anySynced)
      {
        VOXA::dataService.syncAll();
        Serial.println("[BackgroundUpload] Sync complete after cleanup.");
      }
    }
  }
}


void setup()
{
  Serial.begin(115200);
  delay(500);

  // ── Wakeup & Power Stability Configuration ──────────────────────────
  pinMode(GPIO_NUM_1, INPUT_PULLUP);
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0 || wakeupCause == ESP_SLEEP_WAKEUP_GPIO)
  {
    Serial.println("[Power] Woken from Deep Sleep via Power Button (GPIO 1)");
  }

  // ── Battery Power Stability: Disable Brownout Detector ─────────────────
  // On battery, voltage sags during WiFi radio bursts + display refresh.
  // The default brownout threshold (~2.5V) triggers false resets.
  // LiPo/18650 batteries are safe above 3.0V; we handle real low-battery
  // via BatteryManager ADC readings instead of the hardware brownout reset.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector via register write
  Serial.println("[Power] Brownout detector disabled (battery mode safe).");

  // Print reset reason — useful for diagnosing WDT vs brownout vs panic
  esp_reset_reason_t reason = esp_reset_reason();
  const char* resetNames[] = {"UNKNOWN","POWERON","EXT","SW","PANIC","INT_WDT","TASK_WDT","WDT","DEEPSLEEP","BROWNOUT","SDIO"};
  if (reason < 11) Serial.printf("[Power] Last reset reason: %s\n", resetNames[reason]);

  Serial.printf("PSRAM Found: %s\n", psramFound() ? "YES" : "NO");
  Serial.printf("PSRAM Total Size: %d bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM Free Size: %d bytes\n", ESP.getFreePsram());
  Serial.printf("SRAM Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=================================");

  Serial.println("[Startup] Display");
  Display::begin();
  touch.begin();
  PowerManager::instance().begin(0); // 0 = Auto-sleep disabled (keeps display ON permanently)
  Serial.println("[Startup] Display & PowerManager ready");

  // Show boot screen immediately to give visual feedback
  boot.show();

  Serial.println("[Startup] AudioManager (MAX98357A I2S Mono Speaker System)");
  AudioManager::instance().begin();
  AudioManager::instance().playBootMelody();
  AudioManager::instance().playBackgroundMusicLoopAsync();
  Serial.println("[Startup] AudioManager ready & background music loop active");

  Serial.println("[Startup] Storage Subsystem (SPIFFS — cloud-primary mode)");
  delay(100);
  storageManager.begin(); // mounts SPIFFS only; SD card adapter removed


  Serial.println("[Startup] Preferences");
  apiClient.begin();
  apiClient.setBaseUrl("http://192.168.0.148:8000"); // force-update saved backend IP to current network
  wifiManager.begin();
  Serial.println("[Startup] Preferences ready");

  Serial.println("[Startup] WiFi");
  bool wifiConnected = false;
  if (wifiManager.shouldForcePortal())
  {
    Serial.println("[WiFi] Force portal flag set. Launching captive portal...");
    wifiManager.clearForcePortal();
  }
  else if (wifiManager.hasSavedCredentials())
  {
    Serial.println("[WiFi] Saved credentials found. Connecting in background...");
    wifiManager.connect();
    activeScreen = ScreenId::Home;
    wifiConnected = true;
  }

  if (!wifiConnected)
  {
    Serial.println("[WiFi] Entering Setup Mode (Captive Portal)...");
    wifiManager.startPortal();

    bool skipSetup = false;
    bool skipPressed = false;

    auto drawSetupScreen = [](const char *status, bool showSkipPressed)
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

      const char *qrText = "WIFI:S:VOXA-Setup;T:WPA;P:12345678;;";
      const char *stepText = "1. Scan to Connect";

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
      AudioManager::instance().playConnectedChime();
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

  Serial.println("[Startup] TimeService");
  timeService.begin();
  Serial.println("[Startup] TimeService ready");

  Serial.println("[Startup] Services");
  dataService.begin();
  Serial.println("[Startup] Services ready");

  Serial.println("[Startup] HomeScreen");
  home.begin();
  Serial.println("[Startup] HomeScreen ready");

  Serial.println("[Startup] ReminderManager");
  ReminderManager::instance().begin();
  // ReminderManager::instance().runTestScenarios(); // Disabled mock boot alarm
  Serial.println("[Startup] ReminderManager ready");

  Serial.println("[Startup] ButtonService (Physical Long-Press Record Button on GPIO 1)");
  buttonService.begin(PHYSICAL_BUTTON_PIN);
  Serial.println("[Startup] ButtonService ready");

  Serial.println("Boot complete. Starting main screen loop...");
  xTaskCreatePinnedToCore(backgroundUploadTask, "BgUpload", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(backgroundDataSyncTask, "BgDataSync", 8192, nullptr, 1, nullptr, 0);
}

void loop()
{
  // Touch-to-Wake: Tapping the screen while sleeping immediately restores backlight
  if (PowerManager::instance().isSleeping())
  {
    uint16_t tx = 0, ty = 0;
    if (touch.getPoint(tx, ty))
    {
      PowerManager::instance().wakeup();
    }
  }

  PowerManager::instance().tick();

  if (ButtonService::isDirectRecordRequested())
  {
    activeScreen = ScreenId::Record;
  }

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
  case ScreenId::RecordingsLibrary:
    Serial.println("Opening Recordings Library Screen...");
    nextScreen = recordingsLibraryScreen.show(touch);
    break;
  case ScreenId::AudioPlayer:
    Serial.println("Opening Audio Player Screen...");
    nextScreen = audioPlayerScreen.show(touch);
    break;
  case ScreenId::WiFiSettings:
    Serial.println("Opening Wi-Fi Settings Screen...");
    nextScreen = wifiSettingsScreen.show(touch);
    break;
  case ScreenId::BluetoothSettings:
    Serial.println("Opening Bluetooth Settings Screen...");
    nextScreen = bluetoothSettingsScreen.show(touch);
    break;
  case ScreenId::TextInput:

    Serial.println("Opening Text Input Screen...");
    nextScreen = textInputScreen.show(touch);
    break;
  case ScreenId::Tasks:
    Serial.println("Opening Tasks Screen...");
    nextScreen = tasksScreen.show(touch);
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

  // Tick the Reminder Manager
  ReminderManager::instance().tick();

  delay(10);
}