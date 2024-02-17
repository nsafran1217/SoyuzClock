#include <Arduino.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <time.h>

#include <SoyuzDisplay.h>

#ifdef ENABLE_SOUND
#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#endif

#define ENABLE_WIFI

#ifdef ENABLE_WIFI
#include <WiFiManager.h>
#endif

// EEPROM ADDRESSES
#define EEPROM_CRC_ADDRESS 0
#define SETTINGS_ADDRESS 4

// pin definitions
#define MAX_DATA_PIN 25
#define MAX_CLK_PIN 26
#define MAX_LOAD_PIN 27

#define I2S_SD_PIN 2
#define I2S_DIN_PIN 4
#define I2S_BCLK_PIN 15
#define I2S_LRC_PIN 13

#define SD_CS_PIN 5

#define RTC_SCL_PIN 22
#define RTC_SDA_PIN 21

#define RUN_CORRECT_SW_PIN 39 // VN
#define OP_SW_PIN 34
#define ON_SW_PIN 35
#define START_STOP_BUT_PIN 33
#define ENTER_BUT_PIN 32

// Setup Devices
SoyuzDisplay display = SoyuzDisplay(MAX_DATA_PIN, MAX_CLK_PIN, MAX_LOAD_PIN);
#ifdef ENABLE_SOUND
// Audio and SD card
I2SStream i2s;                                           // final output of decoded stream
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier;
File audioFile;
#endif

// Mutexs
const TickType_t delay500ms = pdMS_TO_TICKS(500);
// SemaphoreHandle_t timeChangedMutex;
SemaphoreHandle_t displayMutex;

// Global Vars
unsigned long lastButtonPress = 0;
// DateTime Vars
uint8_t hour = 0, minute = 0, second = 0, month = 0, day = 0;
uint8_t alarmHour = 0, alarmMinute = 0, alarmSecond = 0;
uint8_t stopWatchMinute = 0, stopWatchSecond = 0;
int year = 0;
int timeDots = 0;
int lastsecond = -1;
int lastsecondTime = -1;
int lastsecondStopWatch = -1;

// Struct for clock user settings
struct DeviceSettings
{
  bool twelveHourMode; // used only in normal mode
  bool enableSoundOutput;
  char ntpServer[50];
  long gmtOffset_sec;
  int daylightOffset_sec;
  enum modes
  {
    emulationMode,
    realMode
  };
  modes defualtMode;
  modes currentMode;
};
DeviceSettings settings;

DeviceSettings::modes clockMode; // are we emulating the real thing?
bool alarmEnable = false;
int stopWatchMode = 0; // 0 = reset, 1= start, 2=stop
bool stopWatchRunning = false;
bool timeChanged = false;

// FUNCTIONS
void emulationMode();
void normalMode();
boolean readButton(uint8_t pin);
void updateDateTimeTask(void *parameter);
void displayTime();
void displayDate();
void displayAlarm();
void stopWatchTask(void *parameter);
void wifiManagerSetup(int mode);

bool setTime(int time[]); // 1 we set time, 0 we exited without changing time

boolean isBetweenHours(int hour, int displayOffHour, int displayOn);
uint32_t calculateCRC(const DeviceSettings &settings);
bool readEEPROMWithCRC(DeviceSettings &settings);
void writeEEPROMWithCRC(const DeviceSettings &settings);
void initWiFi();

void setup()
{
  delay(50);
  Serial.begin(115200);
  Serial.println("ON");

#ifdef ENABLE_WIFI
  unsigned long resetTime = millis();
  while (!digitalRead(ENTER_BUT_PIN))
  {
    // display.writechar("RESET");
    //  not implemented
    if (millis() > resetTime + 5000UL)
    {                      // if held down for 5 seconds
      wifiManagerSetup(0); // enter wifi manager and reset settings
    }
  }
  wifiManagerSetup(1);
#endif
  EEPROM.begin(128);
  // bool settingsValid = readEEPROMWithCRC(settings);
  bool settingsValid = false;
  if (!settingsValid)
  {
    Serial.println("CRC BAD");
    settings.twelveHourMode = true;
    strcpy(settings.ntpServer, "pool.ntp.org");
    settings.gmtOffset_sec = -18000;
    settings.daylightOffset_sec = 3600;
    settings.currentMode = DeviceSettings::emulationMode;
    settings.defualtMode = DeviceSettings::emulationMode;

    writeEEPROMWithCRC(settings);
    EEPROM.commit();
  }
  else
  {
    Serial.println("CRC GOOD");
  }
  // setup input pins
  pinMode(RUN_CORRECT_SW_PIN, INPUT);
  pinMode(OP_SW_PIN, INPUT);
  pinMode(ON_SW_PIN, INPUT);
  pinMode(START_STOP_BUT_PIN, INPUT_PULLUP);
  pinMode(ENTER_BUT_PIN, INPUT_PULLUP);

  displayMutex = xSemaphoreCreateMutex();

  clockMode = settings.currentMode;

  if (settings.currentMode != settings.defualtMode) // we booted into a different mode at the request of the user
  {
    settings.currentMode = settings.defualtMode;
    writeEEPROMWithCRC(settings); // put it back to default
    Serial.println("Setting defualt mode back");
  }

  if (clockMode == DeviceSettings::emulationMode)
  {
    struct tm timeinfo;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_year = 124; // 2024
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1; // Day 1 of January
    time_t t = mktime(&timeinfo);
    struct timeval tv = {.tv_sec = t};
    settimeofday(&tv, nullptr); // Set the system time
    timeDots = 1;
    xTaskCreate(updateDateTimeTask, "updateDateTimeTask", 4096, NULL, 1, NULL);
  }
  else
  {
    // connect to WiFi
    // init and get the time
    configTime(settings.gmtOffset_sec, settings.daylightOffset_sec, settings.ntpServer);
    // updateDateTime();
  }

// setCpuFrequencyMhz(80); // slow down for power savings
#ifdef ENABLE_SOUND
  // SD Card and audio stuff

  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup file
  SD.begin();
  audioFile = SD.open("/sound2.mp3");

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.pin_bck = 15;
  config.pin_ws = 13;
  config.pin_data = 4;
  i2s.begin(config);
  pinMode(2, OUTPUT);

  // setup I2S based on sampling rate provided by decoder
  decoder.setNotifyAudioChange(i2s);
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);

#endif
}

void loop()
{
  // updateDateTime(); // update the time vars
  //   main logic loop
  //   so what do we need to do?
  //   well, we need to read 3 switches and execute the correct actions.
  //   in addition, we need to read 2 buttons and determine what action they take based on some of the switches
  //   we need to work on implementing the soyuz functionality, then implement extra functionality
  //   the function of the buttons should depend on the current mode, either emulation or normal
#ifdef ENABLE_SOUND
  if (!copier.copy())
  {
    stop();
  }
#endif

  if (clockMode == DeviceSettings::emulationMode)
  {
    emulationMode();
  }
  else // modern mode
  {
  }
}

void emulationMode()
{
  if (digitalRead(ON_SW_PIN)) // BKL, On Off Switch, ON
  {
  }
  else // OFF
  {
  }

  if (digitalRead(RUN_CORRECT_SW_PIN)) // RUN
  {
    if (digitalRead(OP_SW_PIN)) // current time
    {
      displayTime();
    }
    else // OP
    {
      displayAlarm();
      delay(50);
    }
  }
  else // CORRECTION
  {
    if (digitalRead(OP_SW_PIN)) // current time
    {
      int timeArr[3] = {hour, minute, second};
      if (setTime(timeArr))
      {
        struct tm timeinfo;
        timeinfo.tm_hour = timeArr[0];
        timeinfo.tm_min = timeArr[1];
        timeinfo.tm_sec = timeArr[2];
        timeinfo.tm_year = 124; // 2024
        timeinfo.tm_mon = 0;
        timeinfo.tm_mday = 1; // Day 1 of January
        time_t t = mktime(&timeinfo);
        struct timeval tv = {.tv_sec = t};
        settimeofday(&tv, nullptr); // Set the system time
      }
    }
    else // OP
    {
      int timeArr[3] = {alarmHour, alarmMinute, alarmSecond};

      if (setTime(timeArr))
      {
        alarmEnable = true; // alarm is not enabled until the alarm has been set
        alarmHour = timeArr[0];
        alarmMinute = timeArr[1];
        alarmSecond = timeArr[2];
      }
    }
  }
  // stop watch secition
  if (readButton(START_STOP_BUT_PIN)) // stop watch button pressed
  {
    stopWatchMode++;
    if (stopWatchMode > 2)
      stopWatchMode = 0;
    // do some cleanup when the mode changes
    switch (stopWatchMode)
    {
    case 0:
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
      {
        display.blankSmallDisplay();
        xSemaphoreGive(displayMutex);
      }
      break;
    case 1:
      if (!stopWatchRunning)
      {
        stopWatchRunning = true;
        stopWatchMinute = 0;
        stopWatchSecond = 0;
        xTaskCreate(stopWatchTask, "stopWatchTask", 4096, NULL, 1, NULL);
        // stop watch task runs in background
      }
      break;
    case 2:
      // task checks this var. will stop when false
      stopWatchRunning = false;
      break;
    }
  }

  // alarm section
  if (alarmEnable)
  {
    if (alarmMinute == minute && alarmSecond == second)
    {
      // TODO: Trigger alarm
    }
  }
}

boolean readButton(uint8_t pin) // true if button pressed
{
  // Read the button state
  uint8_t btnState = digitalRead(pin);
  // If we detect LOW signal, button is pressed
  if (btnState == LOW)
  {
    // if 50ms have passed since last LOW pulse, it means that the
    // button has been pressed, released and pressed again
    if (millis() - lastButtonPress > 50)
    {
      lastButtonPress = millis();
      return true;
    }
    else
    {
      lastButtonPress = millis();
      return false;
    }
    // Remember last button press event
  }
  else
  {
    return false;
  }
}
void setVfdMatrixTransition()
{
  // uint8_t digits[3] = {settings.matrix_transition, 255, settings.vfd_transition};
  // uint8_t minValues[3] = {0, 0, 0};
  // uint8_t maxValues[3] = {INS1Matrix::num_enums - 1, 0, IV17::num_enums - 1};
  // userInputClock(digits, minValues, maxValues, ALLOFF);
  // settings.matrix_transition = (INS1Matrix::TransitionMode)digits[0];
  // settings.vfd_transition = (IV17::TransitionMode)digits[2];
  writeEEPROMWithCRC(settings);
}

void updateDateTimeTask(void *parameter)
{
  struct tm timeinfo;
  int i = 0;
  while (1)
  {
    i = 0;
    delay(10);
    while (!getLocalTime(&timeinfo))
    {
      i++;
      Serial.println("Failed to obtain time");
      if (i > 10)
      {
        esp_restart(); // just reboot and try again
      }
    }
    int newSecond = timeinfo.tm_sec;

    if (lastsecond != newSecond) // only call if time has changed
    {
      lastsecond = newSecond;
      hour = timeinfo.tm_hour;
      minute = timeinfo.tm_min;
      second = timeinfo.tm_sec;
      year = timeinfo.tm_year + 1900;
      month = timeinfo.tm_mon + 1;
      day = timeinfo.tm_mday;
    }
  }
}
void displayTime()
{

  if (lastsecondTime != second) // only call if time has changed
  {
    lastsecondTime = second;
    Serial.printf("%02d/%02d/%d %02d:%02d:%02d\n", month, day, year, hour, minute, second); // debug
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
    {
      display.writeTimeToDisplay(hour, minute, second, timeDots);
      xSemaphoreGive(displayMutex);
    }
  }
}
void displayDate()
{
  Serial.printf("%02d/%02d/%d %02d:%02d:%02d\n", month, day, year, hour, minute, second); // debug
}

void displayAlarm()
{
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
  {
    display.writeTimeToDisplay(alarmHour, alarmMinute, alarmSecond, timeDots);
    xSemaphoreGive(displayMutex);
  }
}

// updates display for stop watch when second changes
// used in emulation mode only
void stopWatchTask(void *parameter)
{
  while (stopWatchRunning)
  {
    delay(10);
    if (lastsecondStopWatch != second) // only call if time has changed
    {
      lastsecondStopWatch = second;
      stopWatchSecond++;
      if (stopWatchSecond > 59)
      {
        stopWatchSecond = 0;
        stopWatchMinute++;
      }
      if (stopWatchMinute > 99)
        stopWatchMinute = 0;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
      {
        display.writeTimeToSmallDisplay(stopWatchMinute, stopWatchSecond, 0);
        Serial.printf("              %02d:%02d\n", stopWatchMinute, stopWatchSecond);
        xSemaphoreGive(displayMutex);
      }
    }
  }
  vTaskDelete(NULL);
}

bool setTime(int time[])
{
  // Array to store current values for each field
  int currentValues[6] = {0};
  // max value of each field
  int maxVal[6] = {2, 9, 5, 9, 5, 9};

  // Variables to handle button press timing
  unsigned long pressStartTime = 0;
  bool buttonPressed = false;
  int fieldIndex = 0;
  unsigned long lastIncrementTime = 0; // Timer to control the increment rate
  bool didWeSetTime = false;

  while (fieldIndex < 6 && !digitalRead(RUN_CORRECT_SW_PIN)) // if move back to run mode, exit
  {
    // we continute display time until a button press is detected, then we stop
    if (!didWeSetTime)
    {
      if (digitalRead(OP_SW_PIN)) // current time
      {
        // updateDateTime();
        displayTime();
      }
      else // OP
      {
        displayAlarm();
      }
    }
    else
    {
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
      {
        display.writeTimeToDisplay(
            currentValues[0] * 10 + currentValues[1],
            currentValues[2] * 10 + currentValues[3],
            currentValues[4] * 10 + currentValues[5],
            timeDots);
        xSemaphoreGive(displayMutex);
      }
    }
    // Check if the button is pressed (with debouncing)
    if (digitalRead(ENTER_BUT_PIN) == LOW)
    {
      didWeSetTime = true; // if we press button, then the time is set on exit
      if (!buttonPressed)
      {
        // Button pressed for the first time
        pressStartTime = millis();
        buttonPressed = true;
        Serial.println("Button pressed");
      }
      else
      {

        // Check if it's time to increment the value
        // Wait for 1500ms before starting to increment values
        if (millis() - lastIncrementTime >= 1000 && millis() - pressStartTime >= 1500)
        {
          currentValues[fieldIndex]++;
          if (currentValues[fieldIndex] > maxVal[fieldIndex])
            currentValues[fieldIndex] = 0; // Wrap around if exceeding 9
          if (fieldIndex == 1 && currentValues[fieldIndex] > 3 && currentValues[0] == 2)
            currentValues[fieldIndex] = 0;
          Serial.printf("Field %d value: %d\n", fieldIndex, currentValues[fieldIndex]);
          lastIncrementTime = millis();

          if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(5)))
          {
            display.writeTimeToDisplay(
                currentValues[0] * 10 + currentValues[1],
                currentValues[2] * 10 + currentValues[3],
                currentValues[4] * 10 + currentValues[5],
                timeDots);
            xSemaphoreGive(displayMutex);
          }
        }
      }
    }
    else
    {
      // Button released
      if (buttonPressed)
      {
        // Button was pressed
        fieldIndex++;
        Serial.printf("Moving to field %d\n", fieldIndex);
        // Print current field being edited
        if (fieldIndex < 6)
          Serial.printf("Editing field %d\n", fieldIndex);

        buttonPressed = false;
      }
    }

    // Update the original time array
    int j = 0;
    for (int i = 0; i < 3; i++)
    {
      time[i] = currentValues[j] * 10 + currentValues[j + 1];
      j++;
      j++;
    }

    // Delay to prevent too fast changes
    delay(50);
  }
  // if still in correction
  while (!digitalRead(RUN_CORRECT_SW_PIN))
  {
    // TODO, something. for now just hold execution
  }

  return didWeSetTime;
}

bool isBetweenHours(int hour, int displayOffHour, int displayOnHour)
{
  // Check if the hour is greater than or equal to displayOffHour
  // and less than displayOnHour, taking into account the 24-hour clock.
  if (displayOffHour < displayOnHour)
  {
    return (hour >= displayOffHour && hour < displayOnHour);
  }
  else
  {
    // Handle the case when displayOnHour crosses midnight
    return (hour >= displayOffHour || hour < displayOnHour);
  }
}
uint32_t calculateCRC(const DeviceSettings &settings) // Calculate CRC32 (simple algorithm)
{
  uint32_t crc = 0;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(&settings);
  int dataSize = sizeof(settings);

  for (int i = 0; i < dataSize; ++i)
  {
    crc ^= (uint32_t)data[i] << 24;
    for (int j = 0; j < 8; ++j)
    {
      if (crc & 0x80000000)
      {
        crc = (crc << 1) ^ 0x04C11DB7; // CRC-32 polynomial
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}
bool readEEPROMWithCRC(DeviceSettings &settings) // Read EEPROM and verify CRC
{
  uint32_t storedCRC;
  EEPROM.get(EEPROM_CRC_ADDRESS, storedCRC);

  DeviceSettings tempSettings;
  EEPROM.get(SETTINGS_ADDRESS, tempSettings);

  if (storedCRC == calculateCRC(tempSettings))
  {
    settings = tempSettings;
    return true; // CRC matches, data is valid
  }
  else
  {
    return false;
  }
}
void writeEEPROMWithCRC(const DeviceSettings &settings) // Write EEPROM with CRC
{
  // Write settings to EEPROM
  EEPROM.put(SETTINGS_ADDRESS, settings);

  // Calculate CRC for the data in EEPROM
  uint32_t calculatedCRC = calculateCRC(settings);

  // Write CRC to EEPROM
  EEPROM.put(EEPROM_CRC_ADDRESS, calculatedCRC);
}

void wifiManagerSetup(int mode)
{
#ifdef ENABLE_WIFI
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.setDebugOutput(true);

  WiFiManager wm;
  WiFiManagerParameter ntpServerCustomField;
  WiFiManagerParameter gmtOffsetCustomField;
  WiFiManagerParameter daylightOffsetCustomField;
  WiFiManagerParameter defaultModeCustomField;
  WiFiManagerParameter twelveHourCustomField;

  new (&ntpServerCustomField) WiFiManagerParameter("ntp_server", "NTP Server", "pool.ntp.org", 50);
  new (&gmtOffsetCustomField) WiFiManagerParameter("gmt_offset", "GMT Offset (secs) - EST default", "-18000", 50);
  new (&daylightOffsetCustomField) WiFiManagerParameter("daylightOffset", "Daylight Time Offset (secs)", "3600", 50);

  const char *custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  switch (mode)
  {
  case 0:               // hard reset
    wm.resetSettings(); // fall through to normal setup

  case 1: // normal startup
    bool res;
    res = wm.autoConnect("Soyuz");

    if (!res)
    {
      Serial.println("Failed to connect");
      // ESP.restart();
    }
    else
    {
      Serial.println("connected...yeey :)");
    }

    break;
  case 2: // adhoc settings change

    // set configportal timeout
    wm.setConfigPortalTimeout(300);

    if (!wm.startConfigPortal("Soyuz"))
    {
      Serial.println("failed to connect and hit timeout");
    }

    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    break;
  }
#endif
}