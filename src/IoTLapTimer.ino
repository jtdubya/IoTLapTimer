/*
 Name:		IoTLapTimer.ino
 Created:	3/20/2020 2:10:44 PM
 Author:	john
*/
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <Wire.h>
#include "LapTimer.h"

// IoT Lap Timer
// The LED is used to communicate state to the user
//
// LED States:
//
// 1. Initial Wifi Connection
//		1.1 - Connected: LED off
//		1.2 - Not Connected: 2 quick blinks
//		1.3 - IP address changed: continuous quick blinks
// 2. After initial connection
//      2.1 - Registered: LED off - Numeric displays all dashes
//		2.2 - Not Registered: 1 quick blink, 1 long blink
//		2.4 - HTTP error (could be caused by things like the server is not up or wrong url): 2 quick blinks, 1 long blink
//		2.3 - Registration closed (max number of participants has been reached or race in progress): 5 quick blinks, 1 long blink

// Numeric Display States:
//
// 1. Registered - Lane Number prefaced with "L"
// 2. Start Countdown - Minus sign followed by seconds until start
// 3. Race in Progress - Time Displayed
// 4. Race finish - Rotates through:
//                  - Place prefaced with the letter "P"
//                  - Overall Time
//                  - Number of laps prefaced with the letters "LC"

void setup()
{
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
    TurnLedOff();
    _timerState = NOT_REGISTERED;
    WiFi.mode(WIFI_STA);
    _wifiMulti.addAP(_netConfig.ssid, _netConfig.key);
    pinMode(TRIGGER_ADDRESS, OUTPUT);
    pinMode(ECHO_ADDRESS, INPUT);
    _display.begin(DISPLAY_ADDRESS);
    _display.writeDigitRaw(0, DASH);
    _display.writeDigitRaw(1, DASH);
    _display.writeDigitRaw(3, DASH);
    _display.writeDigitRaw(4, DASH);
    _display.drawColon(false);
    _display.writeDisplay();
}

void loop()
{
    if (_wifiMulti.run() == WL_CONNECTED)
    {
        HandleTimerState();
    }
    else
    {
        Serial.println("NOT connected");
        BlinkLED(QUICK_BLINK_DURATION, 2);
        delay(LAP_COUNT_DISPLAY_DELAY_MS);
    }

    delay(LOOP_DELAY_MS);
}

void CheckIPAddress()
{
    IPAddress wifiIPAddress = WiFi.localIP();

    if (!_ipAddress.isSet() && wifiIPAddress.isSet())
    {
        _ipAddress = wifiIPAddress;
    }
    else if (_ipAddress != wifiIPAddress)
    {
        // this should not happen since the IP should be tied to the device
        // but, if it does happen then it is in a unrecoverable state (for now)
        // since the server uses IP address to register
        // if this occurs then we need to add a update ip address method to the api
        Serial.println("IP address has changed");
        Serial.println("OLD IP: ");
        Serial.println(_ipAddress);
        Serial.println("New IP: ");
        Serial.println(wifiIPAddress);
        BlinkLED(QUICK_BLINK_DURATION, QUICK_BLINK_DURATION, INT_MAX);
    }
}

void TurnLedOn()
{
    digitalWrite(LED_BUILTIN, LOW);
    _ledOn = true;
}

void TurnLedOff()
{
    digitalWrite(LED_BUILTIN, HIGH);
    _ledOn = false;
}

void BlinkLED(int onDuration, int blinkCount)
{
    BlinkLED(onDuration, BLINK_INTERVAL, blinkCount);
}

void BlinkLED(int onDuration, int blinkInterval, int blinkCount)
{
    if (onDuration < 100)
    {
        onDuration = 100; // 100ms is shortest blink duration
    }

    for (int i = 0; i < blinkCount; i++)
    {
        TurnLedOn();
        delay(onDuration);
        TurnLedOff();
        delay(blinkInterval);
    }
}

void HandleTimerState()
{
    switch (_timerState)
    {
    case NOT_REGISTERED:
        CheckIPAddress();
        RegisterTimer();
        break;
    case REGISTERED:
        WaitForStartCountdown();
        break;
    case RACE_START_COUNTDOWN:
        CountdownToRaceStart();
        break;
    case RACE_IN_PROGRESS:
        HandleRace();
        break;
    case RACE_FINISH:
        GetRaceResult();
        break;
    default:
        Serial.println("Unhandled timer state: ");
        Serial.println(_timerState);
        delay(5000);
        break;
    }
}

HttpResponse SendGetRequest(String resource)
{
    String endpoint = (String)_netConfig.endpointBase + resource;
    WiFiClient wifiClient;
    HTTPClient httpClient;
    HttpResponse getResponse;

    Serial.print("GET ");
    Serial.println(endpoint);
    if (httpClient.begin(wifiClient, endpoint))
    {
        getResponse.httpCode = httpClient.GET();

        if (getResponse.httpCode > 0)
        { /// HTTP client errors are negative
            Serial.printf("HTTP GET success, code: %d\n", getResponse.httpCode);
            getResponse.body = httpClient.getString();
            auto size = sizeof(getResponse.body);
            Serial.print("response Body size: ");
            Serial.println(size * 8);
            Serial.print("response Body: ");
            Serial.println(getResponse.body);
        }
        else
        {
            Serial.printf("HTTP GET failed, error: %s\n", httpClient.errorToString(getResponse.httpCode).c_str());
            BlinkLED(QUICK_BLINK_DURATION, 2);
            BlinkLED(LONG_BLINK_DURATION, 1);
        }

        httpClient.end();
    }

    return getResponse;
}

JsonResponse DeserializeJsonResponse(String jsonBody)
{
    JsonResponse jsonResponse;
    DeserializationError deserializationError = deserializeJson(jsonResponse.document, jsonBody);

    if (deserializationError)
    {
        jsonResponse.deserializationSuccess = false;
        Serial.print(F("deserializeJson() of string json response failed with code: "));
        Serial.println(deserializationError.c_str());
    }
    else
    {
        jsonResponse.deserializationSuccess = true;
    }

    return jsonResponse;
}

void RegisterTimer()
{
    String registerEndpoint = "/Register/" + _ipAddress.toString();
    HttpResponse getResponse = SendGetRequest(registerEndpoint);

    if (getResponse.httpCode == HTTP_CODE_OK)
    {
        JsonResponse jsonResponse = DeserializeJsonResponse(getResponse.body);

        if (jsonResponse.deserializationSuccess)
        {
            int id = jsonResponse.document["id"];
            const char* message = jsonResponse.document["responseMessage"];
            Serial.print(F("register response message: "));
            Serial.println(message);

            if (id > 0)
            {
                _id = id;
                _timerState = REGISTERED;
                _display.writeDigitRaw(0, LETTER_L);

                if (_id < 10)
                {
                    _display.writeDigitRaw(1, BLANK);
                    _display.writeDigitRaw(3, BLANK);
                    _display.writeDigitNum(4, _id);
                }
                else if (_id >= 10 && _id < 100)
                {
                    _display.writeDigitRaw(1, BLANK);
                    _display.writeDigitNum(3, (_id / 10));
                    _display.writeDigitNum(4, (_id % 10));
                }
                else if (_id >= 100 && _id < 1000)
                {
                    _display.writeDigitNum(1, (_id / 100));
                    _display.writeDigitNum(3, (_id / 10));
                    _display.writeDigitNum(4, (_id % 10));
                }

                _display.drawColon(false);
                _display.writeDisplay();
            }
            else
            {
                BlinkLED(QUICK_BLINK_DURATION, 5);
                BlinkLED(LONG_BLINK_DURATION, 1);
            }
        }
    }
}

void WaitForStartCountdown()
{
    HttpResponse response = SendGetRequest("/GetRaceState");

    if (response.httpCode == HTTP_CODE_OK)
    {
        JsonResponse jsonResponse = DeserializeJsonResponse(response.body);

        if (jsonResponse.deserializationSuccess)
        {
            String state = jsonResponse.document["stateName"];

            if (state.equals("StartCountdown"))
            {
                _timerState = RACE_START_COUNTDOWN;
            }
        }
    }
}

void CountdownToRaceStart()
{
    if (_millisecondsToRaceStartFromServer == -1) // haven't received countdown duration from server yet
    {
        HttpResponse response = SendGetRequest("/GetTimeUntilRaceStart");

        if (response.httpCode == HTTP_CODE_OK)
        {
            _countdownStart = std::chrono::high_resolution_clock::now();
            JsonResponse jsonResponse = DeserializeJsonResponse(response.body);

            if (jsonResponse.deserializationSuccess)
            {
                _millisecondsToRaceStartFromServer = jsonResponse.document["millisecondsUntilStart"];
                _numberOfLaps = jsonResponse.document["numberOfLaps"];
            }
        }
    }

    if (_millisecondsToRaceStartFromServer == 0)
    {
        _timerState = RACE_IN_PROGRESS;
        DisplayTime(std::chrono::nanoseconds(0));
    }
    else
    {
        std::chrono::nanoseconds elapsedNanoseconds = std::chrono::high_resolution_clock::now() - _countdownStart;
        int64_t elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedNanoseconds).count();

        if ((elapsedMilliseconds >= _millisecondsToRaceStartFromServer))
        {
            _timerState = RACE_IN_PROGRESS;
            DisplayTime(std::chrono::nanoseconds(0));
        }
        else
        {
            int64_t millisUntilStart = _millisecondsToRaceStartFromServer - elapsedMilliseconds;
            DisplayTime(std::chrono::nanoseconds(millisUntilStart * ONE_MILLION));
        }
    }
}

void PostLapTimeToServer(std::chrono::nanoseconds lapTime)
{
    int64_t lapTimeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(lapTime).count();

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["ipAddress"] = _ipAddress.toString();
    jsonDoc["lapTime"] = lapTimeMillis;
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    Serial.print("Lap time json: ");
    Serial.println(jsonString);

    String lapResultEndpoint = "/AddLapResultInMilliseconds";
    String endpoint = (String)_netConfig.endpointBase + lapResultEndpoint;
    WiFiClient wifiClient;
    HTTPClient httpClient;
    HttpResponse httpResponse;

    if (httpClient.begin(wifiClient, endpoint))
    {
        httpClient.addHeader("Content-Type", "application/json");
        httpResponse.httpCode = httpClient.POST(jsonString);

        if (httpResponse.httpCode > 0)
        { /// HTTP client errors are negative
            Serial.printf("HTTP POST success, code: %d\n", httpResponse.httpCode);
            httpResponse.body = httpClient.getString();
            auto size = sizeof(httpResponse.body);
            Serial.print("response Body size: ");
            Serial.println(size * 8);
            Serial.print("response Body: ");
            Serial.println(httpResponse.body);
        }
        else
        {
            Serial.printf("HTTP POST failed, error: %s\n", httpClient.errorToString(httpResponse.httpCode).c_str());
            BlinkLED(QUICK_BLINK_DURATION, 2);
            BlinkLED(LONG_BLINK_DURATION, 1);
        }

        httpClient.end();
    }
}

void GetRaceResult()
{
    HttpResponse response = SendGetRequest("/GetLastRaceResultById/" + String(_id));

    if (response.httpCode == HTTP_CODE_OK)
    {
        JsonResponse jsonResponse = DeserializeJsonResponse(response.body);

        if (jsonResponse.deserializationSuccess)
        {
            String message = jsonResponse.document["responseMessage"];

            if (message.equals("success"))
            {
                int place = jsonResponse.document["place"];
                int64_t overallTimeMilliseconds = jsonResponse.document["overallTimeMilliseconds"];
                int64_t fastestLapMilliseconds = jsonResponse.document["fastestLapMilliseconds"];
                int fastestLapNumber = jsonResponse.document["fastestLapNumber"];

                Serial.print("overall: ");
                Serial.println((double)overallTimeMilliseconds);
                Serial.print("fastestLap: ");
                Serial.println((double)fastestLapMilliseconds);

                std::chrono::nanoseconds overallTimeNanoseconds = std::chrono::nanoseconds(overallTimeMilliseconds * ONE_MILLION);
                std::chrono::nanoseconds fastestLapNanoseconds = std::chrono::nanoseconds(fastestLapMilliseconds * ONE_MILLION);

                Serial.print("overallTimeNano: ");
                Serial.println((double)overallTimeNanoseconds.count());
                Serial.print("fastestLapNano: ");
                Serial.println((double)fastestLapNanoseconds.count());

                for (int i = 0; i < 2; i++)
                {
                    // Display Place
                    _display.writeDigitRaw(0, LETTER_P);
                    _display.writeDigitRaw(1, BLANK);
                    _display.writeDigitRaw(3, BLANK);
                    _display.writeDigitNum(4, place);
                    _display.drawColon(false);
                    _display.writeDisplay();
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);

                    // Display Overall Time
                    _display.writeDigitNum(0, 0);
                    _display.writeDigitRaw(1, LETTER_A);
                    _display.writeDigitRaw(3, BLANK);
                    _display.writeDigitRaw(4, BLANK);
                    _display.drawColon(false);
                    _display.writeDisplay();
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);
                    DisplayTime(overallTimeNanoseconds);
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);

                    // Display Fastest Lap
                    _display.writeDigitRaw(0, LETTER_F);
                    _display.writeDigitRaw(1, LETTER_L);
                    _display.writeDigitRaw(3, BLANK);
                    _display.writeDigitRaw(4, BLANK);
                    _display.drawColon(false);
                    _display.writeDisplay();
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);
                    DisplayTime(fastestLapNanoseconds);
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);

                    // Display Fastest Lap Number
                    _display.writeDigitRaw(0, LETTER_F);
                    _display.writeDigitRaw(1, LETTER_L);

                    if (fastestLapNumber < 10)
                    {
                        _display.writeDigitRaw(3, BLANK);
                        _display.writeDigitNum(4, fastestLapNumber);
                    }
                    else if (fastestLapNumber > 10 && fastestLapNumber < 100)
                    {
                        _display.writeDigitNum(3, fastestLapNumber / 10);
                        _display.writeDigitNum(4, fastestLapNumber % 10);
                    }
                    else
                    {
                        // TODO
                    }
                    _display.writeDisplay();
                    _display.drawColon(false);
                    _display.writeDisplay();
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);

                    // Display Place
                    _display.writeDigitRaw(0, LETTER_P);
                    _display.writeDigitRaw(1, BLANK);
                    _display.writeDigitRaw(3, BLANK);
                    _display.writeDigitNum(4, place);
                    _display.drawColon(false);
                    _display.writeDisplay();
                    delay(LAP_COUNT_DISPLAY_DELAY_MS);

                    // reset
                    _lapCount = 1;
                    _millisecondsToRaceStartFromServer = -1;
                    _raceHasStarted = false;
                    _isFirstCarDetection = false;
                    _timerState = REGISTERED;
                }
            }
            else
            {
                _display.writeDigitRaw(0, BLANK);
                _display.writeDigitRaw(1, BLANK);
                _display.writeDigitRaw(3, BLANK);
                _display.writeDigitRaw(4, BLANK);
                _display.drawColon(true);
                _display.writeDisplay();
            }
        }
    }
}

long GetDistanceCentimeters(long duration)
{
    return (duration / 2.0) / 29.1;
}

bool IsDistanceWithinThreshold(long distance)
{
    return (distance > MIN_DISTANCE && distance < MAX_DISTANCE);
}

void DisplayMinutesAndSeconds(int64_t minutes, int64_t secondsWithMinutes)
{
    int64_t seconds = secondsWithMinutes % 60; // modulo out the minutes
    // Display minutes on left two digits
    if (minutes < 10)
    {
        _display.writeDigitNum(0, 0);
        _display.writeDigitNum(1, minutes);
    }
    else
    {
        _display.writeDigitNum(0, (minutes / 10));
        _display.writeDigitNum(1, (minutes % 10));
    }

    // Display seconds on right two digits
    if (seconds < 10)
    {
        _display.writeDigitNum(3, 0);
        _display.writeDigitNum(4, seconds);
    }
    else
    {
        _display.writeDigitNum(3, (seconds / 10));
        _display.writeDigitNum(4, (seconds % 10));
    }

    _display.drawColon(true);
    _display.writeDisplay();
}

void DisplaySecondsAndMilliseconds(int64_t seconds, int64_t millis)
{
    // Display seconds on left two digits
    if (seconds < 10)
    {
        _display.writeDigitNum(0, 0);
        _display.writeDigitNum(1, seconds, true);
    }
    else if (seconds < 100)
    {
        _display.writeDigitNum(0, seconds / 10);
        _display.writeDigitNum(1, seconds % 10, true);
    }

    // Display milliseconds on right two digits
    int twoDigits = (millis % 1000) / 10;
    int thirdDigit = millis % 10;

    if (thirdDigit >= 5) // round to two digits
    {
        twoDigits += 1;
    }

    _display.writeDigitNum(3, twoDigits / 10);
    _display.writeDigitNum(4, twoDigits % 10);

    _display.drawColon(false);
    _display.writeDisplay();
}

void DisplayTime(std::chrono::nanoseconds time)
{
    int64_t lapTimeMinutes = std::chrono::duration_cast<std::chrono::minutes>(time).count();
    int64_t lapTimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(time).count();
    int64_t lapTimeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(time).count() % 1000; // modulo out the seconds

        // Display seconds
    if (lapTimeSeconds < 60)
    {
        DisplaySecondsAndMilliseconds(lapTimeSeconds, lapTimeMillis);
    }
    else if (lapTimeMinutes < 100)
    {
        DisplayMinutesAndSeconds(lapTimeMinutes, lapTimeSeconds);
    }
    else
    {
        _display.writeDigitRaw(0, DASH);
        _display.writeDigitRaw(1, DASH);
        _display.writeDigitRaw(3, DASH);
        _display.writeDigitRaw(4, DASH);
        _display.writeDisplay();
    }
}

void DisplayLapCount(int lapCount)
{
    if (lapCount >= 1000)
    {
        _display.print(lapCount);
    }
    else
    {
        _display.writeDigitRaw(0, LETTER_L);

        if (lapCount < 10)
        {
            _display.writeDigitRaw(1, LETTER_A);
            _display.writeDigitRaw(3, LETTER_P);
            _display.writeDigitNum(4, lapCount);
        }
        else if (lapCount >= 10 && lapCount < 100)
        {
            _display.writeDigitRaw(1, LETTER_P);
            _display.writeDigitNum(3, (lapCount / 10));
            _display.writeDigitNum(4, (lapCount % 10));
        }
        else if (lapCount >= 100 && lapCount < 1000)
        {
            _display.writeDigitNum(1, (lapCount / 100));
            _display.writeDigitNum(2, (lapCount / 10));
            _display.writeDigitNum(3, (lapCount % 10));
        }
    }

    _display.drawColon(false);
    _display.writeDisplay();
    delay(LAP_COUNT_DISPLAY_DELAY_MS);
}

void DisplayLapTime(std::chrono::nanoseconds lapTime)
{
    for (int flashCount = 0; flashCount < 3; flashCount++) {
        DisplayTime(lapTime);
        delay(THIRD_OF_A_SECOND_MS);
        _display.writeDigitRaw(0, BLANK);
        _display.writeDigitRaw(1, BLANK);
        _display.writeDigitRaw(3, BLANK);
        _display.writeDigitRaw(4, BLANK);
        _display.writeDisplay();
        delay(QUARTER_OF_A_SECOND_MS);
    }
}

void HandleRace()
{
    long duration, distance;
    digitalWrite(TRIGGER_ADDRESS, LOW);
    delayMicroseconds(5);

    digitalWrite(TRIGGER_ADDRESS, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIGGER_ADDRESS, LOW);

    duration = pulseIn(ECHO_ADDRESS, HIGH);
    _lapEnd = std::chrono::high_resolution_clock::now();
    std::chrono::nanoseconds lapTime = _lapEnd - _lapStart;
    distance = GetDistanceCentimeters(duration);

    if (IsDistanceWithinThreshold(distance))
    {
        if (lapTime.count() > LAP_LOCKOUT_DURATION_NS)
        {
            if (!_raceHasStarted)
            {
                _raceHasStarted = true;
                _isFirstCarDetection = false;
                _lapStart = std::chrono::high_resolution_clock::now();
                DisplayLapCount(_lapCount);
            }
            else if (_isFirstCarDetection)
            {
                _isFirstCarDetection = false;
                _lapStart = std::chrono::high_resolution_clock::now();
                _lapCount += 1;
                DisplayLapTime(lapTime);

                if (_lapCount > _numberOfLaps)
                {
                    _timerState = RACE_FINISH;
                }
                else
                {
                    DisplayLapCount(_lapCount);
                }

                PostLapTimeToServer(lapTime);
            }
        }
    }
    else if (distance < MAX_THRESHOLD_DISTANCE)
    {
        _isFirstCarDetection = true;
    }

    if (_raceHasStarted)
    {
        DisplayTime(lapTime);
    }
}