#include <Arduino.h>

#include <Arduino.h>
#include <SPIFFS.h>
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

  Serial.println("================================");
  Serial.println("VOXA Firmware Starting...");
  Serial.println("================================");

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

  // Initialize system/RTC clock time to match compile/user baseline local time
  timeService.begin();
  timeService.setTime(12, 39, 41, 7, 7, 2026);

  // Auto-connect Wi-Fi if enabled in saved settings
  Settings settings = settingsService.getSettings();
  if (settings.wifiEnabled)
  {
      wifiManager.connect();
  }

  Serial.println("Boot Screen Finished. Starting main screen loop...");
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
