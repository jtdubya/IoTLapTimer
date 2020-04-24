#pragma once
#include <chrono>
#include <cmath>
#include <string>
#include <sstream>
#include <WString.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "ArduinoJson-v6.15.1.h"
#include "NetworkConfiguration.h"

// Numeric Display Seven segment bit mask
//  _
// |_|
// |_|
//
//B00000001 Top
//B00000010 Top Right
//B00000100 Bottom Right
//B00001000 Bottom
//B00010000 Bottom Left
//B00100000 Top Left
//B01000000 middle bar

// bit mask letters
constexpr uint8_t LETTER_L = B00111000;
constexpr uint8_t LETTER_A = B01110111;
constexpr uint8_t LETTER_P = B01110011;
constexpr uint8_t LETTER_C = B00111001;
constexpr uint8_t DASH = B01000000;
constexpr uint8_t BLANK = B00000000;

constexpr uint8_t TRIGGER_ADDRESS = 12;
constexpr uint8_t ECHO_ADDRESS = 14;
constexpr uint8_t DISPLAY_ADDRESS = 0x70;
constexpr int MIN_DISTANCE = 1;
constexpr int MAX_DISTANCE = 5;
constexpr int LOOP_DELAY_MS = 10;
constexpr int THIRD_OF_A_SECOND_MS = 333;
constexpr int QUARTER_OF_A_SECOND_MS = 250;
constexpr long MAX_THRESHOLD_DISTANCE = 45; // lower to prevent false detects from resetting the first car detection
constexpr int64_t LAP_LOCKOUT_DURATION_NS = 2000000000;
constexpr int64_t LAP_COUNT_DISPLAY_DELAY_MS = 1000;
constexpr long ONE_MILLION = 1000000;

constexpr NetworkConfiguration _netConfig = NetworkConfiguration();
constexpr int QUICK_BLINK_DURATION = 150;
constexpr int LONG_BLINK_DURATION = 750;
constexpr int BLINK_INTERVAL = 250;
constexpr size_t GET_RESPONSE_SIZE = 160;
const String REGISTRATION_CLOSED = "Registration closed";

enum TimerState
{
    NOT_REGISTERED = 0,
    REGISTERED,
    RACE_START_COUNTDOWN,
    RACE_IN_PROGRESS,
    RACE_FINISH
};

int _lapCount = 1;
int _id;
long _millisecondsToRaceStartFromServer = -1;
bool _raceHasStarted = false;
bool _isFirstCarDetection = false;
bool _ledOn;
TimerState _timerState;
ESP8266WiFiMulti _wifiMulti;
IPAddress _ipAddress;
std::chrono::high_resolution_clock::time_point _countdownStart, _lapStart, _lapEnd;
Adafruit_7segment _display = Adafruit_7segment();

struct GetResponse
{
    int httpCode;
    String body; // only set if httpCode is non-negative
};

struct JsonResponse
{
    bool deserializationSuccess = false;
    StaticJsonDocument<GET_RESPONSE_SIZE> document;
};

long GetDistanceCentimeters(long duration);
bool IsDistanceWithinThreshold(long distance);
void DisplayMinutesAndSeconds(int64_t minutes, int64_t secondsWithMinutes);
void DisplaySecondsAndMilliseconds(int64_t seconds, int64_t millis);
void DisplayTime(std::chrono::nanoseconds time);
void DisplayLapCount(int lapCount);