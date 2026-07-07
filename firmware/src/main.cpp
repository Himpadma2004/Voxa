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
#include "services/TimeService.h"
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
  TimeService timeService;
  timeService.setTime(12, 39, 41, 7, 7, 2026);

  Display::begin();

  touch.begin();

  Serial.println("Display Initialized");
  Serial.println("Touch Initialized");

  boot.show();

  Serial.println("Boot Screen Finished. Starting main screen loop...");
}

void loop()
{
  switch (activeScreen)
  {
    case ScreenId::Home:
      activeScreen = home.show(touch);
      break;
    case ScreenId::Reminders:
      Serial.println("Opening Reminder Screen...");
      activeScreen = reminderScreen.show(touch);
      break;
    case ScreenId::Ideas:
      Serial.println("Opening Ideas Screen...");
      activeScreen = ideasScreen.show(touch);
      break;
    case ScreenId::Questions:
      Serial.println("Opening Questions Screen...");
      activeScreen = questionsScreen.show(touch);
      break;
    case ScreenId::Search:
      Serial.println("Opening Search Screen...");
      activeScreen = searchScreen.show(touch);
      break;
    case ScreenId::Record:
      Serial.println("Opening Record Screen...");
      activeScreen = recordScreen.show(touch);
      break;
    case ScreenId::Others:
      Serial.println("Opening Others Screen...");
      activeScreen = othersScreen.show(touch);
      break;
    case ScreenId::Settings:
      Serial.println("Opening Settings Screen...");
      activeScreen = settingsScreen.show(touch);
      break;
    case ScreenId::SyncStatus:
      Serial.println("Opening Sync Status Screen...");
      activeScreen = syncStatusScreen.show(touch);
      break;
    case ScreenId::Detail:
      Serial.println("Opening Detail Screen...");
      activeScreen = detailScreen.show(touch);
      break;
    default:
      activeScreen = ScreenId::Home;
      break;
  }

  delay(10);
}
