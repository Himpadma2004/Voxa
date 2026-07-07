#include <Arduino.h>

#include "display/Display.h"
#include "touch/Touch.h"
#include "screens/BootScreen.h"
#include "screens/HomeScreen.h"
#include "services/TimeService.h"

using namespace VOXA;

Touch touch;
BootScreen boot;
HomeScreen home;

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("================================");
  Serial.println("VOXA Firmware Starting...");
  Serial.println("================================");

  // Initialize system/RTC clock time to match compile/user baseline local time
  TimeService timeService;
  timeService.setTime(12, 39, 41, 7, 7, 2026);

  Display::begin();

  touch.begin();

  Serial.println("Display Initialized");
  Serial.println("Touch Initialized");

  boot.show();

  Serial.println("Boot Screen Finished. Starting Home Screen...");
}

void loop()
{
  ScreenId nextScreen = home.show(touch);

  switch (nextScreen)
  {
    case ScreenId::Reminders:
      Serial.println("Opening Reminder Screen...");
      break;
    case ScreenId::Ideas:
      Serial.println("Opening Ideas Screen...");
      break;
    case ScreenId::Questions:
      Serial.println("Opening Questions Screen...");
      break;
    case ScreenId::Search:
      Serial.println("Opening Search Screen...");
      break;
    case ScreenId::Record:
      Serial.println("Opening Record Screen...");
      break;
    case ScreenId::Others:
      Serial.println("Opening Others Screen...");
      break;
    case ScreenId::Settings:
      Serial.println("Opening Settings Screen...");
      break;
    default:
      break;
  }

  Serial.print("[Navigation] Request transition to Screen ID: ");
  Serial.println(static_cast<int>(nextScreen));

  delay(100);
}
