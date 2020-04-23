/*
 Name:		IoTLapTimer.ino
 Created:	3/20/2020 2:10:44 PM
 Author:	john
*/
#include "LapTimer.h"

// IoT Lap Timer
// The LED is used to communicate state to the user
//
// LED States:
//
// 1. Wifi Connect
//		1.1 - Connected: LED off
//		1.2 - Not Connected: 2 quick blinks
//		1.3 - IP address changed: continuous quick blinks
// 2. Registration
//      2.1 - Registered: LED off
//		2.2 - Not Registered: 1 quick blink, 1 long blink
//		2.4 - HTTP error (could be caused by things like the server is not up or wrong url): 2 quick blinks, 1 long blink
//		2.3 - Registration closed (max number of participants has been reached or race in progress): 5 quick blinks, 1 long blink

void setup()
{
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
    turnLedOff();
    _timerState = NOT_REGISTERED;
    WiFi.mode(WIFI_STA);
    _wifiMulti.addAP(_netConfig.ssid, _netConfig.key);
}

void loop()
{
    if (_wifiMulti.run() == WL_CONNECTED)
    {
        handleTimerState();
    }
    else
    {
        Serial.println("NOT connected");
        blinkLED(QUICK_BLINK_DURATION, 2);
        delay(900);
    }
    delay(100);
}

void checkIPAddress()
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
        blinkLED(QUICK_BLINK_DURATION, QUICK_BLINK_DURATION, INT_MAX);
    }
}

void turnLedOn()
{
    digitalWrite(LED_BUILTIN, LOW);
    _ledOn = true;
}

void turnLedOff()
{
    digitalWrite(LED_BUILTIN, HIGH);
    _ledOn = false;
}

void blinkLED(int onDuration, int blinkCount)
{
    blinkLED(onDuration, BLINK_INTERVAL, blinkCount);
}

void blinkLED(int onDuration, int blinkInterval, int blinkCount)
{
    if (onDuration < 100) {
        onDuration = 100; // 100ms is shortest blink duration
    }

    for (int i = 0; i < blinkCount; i++)
    {
        turnLedOn();
        delay(onDuration);
        turnLedOff();
        delay(blinkInterval);
    }
}

void handleTimerState()
{
    switch (_timerState)
    {
    case NOT_REGISTERED:
        checkIPAddress();
        registerTimer();
        break;
    default:
        Serial.println("Unhandled timer state: ");
        Serial.println(_timerState);
        delay(5000);
        break;
        //REGISTERED
        //START_RACE
        //RACE_IN_PROGRESS
        //RACE_FINISH
    }
}

GetResponse sendGetRequest(String endpoint)
{
    WiFiClient wifiClient;
    HTTPClient httpClient;
    GetResponse getResponse;

    if (httpClient.begin(wifiClient, endpoint))
    {
        getResponse.httpCode = httpClient.GET();

        if (getResponse.httpCode > 0) { /// HTTP client errors are negative
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
            blinkLED(QUICK_BLINK_DURATION, 2);
            blinkLED(LONG_BLINK_DURATION, 1);
        }

        httpClient.end();
    }

    return getResponse;
}

void registerTimer()
{
    String registerEndpoint = (String)_netConfig.apiBase + "/Register/" + _ipAddress.toString();
    GetResponse getResponse = sendGetRequest(registerEndpoint);

    if (getResponse.httpCode == HTTP_CODE_OK) {
        StaticJsonDocument<GET_RESPONSE_SIZE> jsonDoc;
        DeserializationError deserializationError = deserializeJson(jsonDoc, getResponse.body);

        if (deserializationError)
        {
            Serial.print(F("deserializeJson() failed with code: "));
            Serial.println(deserializationError.c_str());
        }
        else
        {
            int id = jsonDoc["id"];
            const char* message = jsonDoc["responseMessage"];
            Serial.print(F("register response message: "));
            Serial.println(message);

            if (id > 0) {
                _id = id;
                _timerState = REGISTERED;
            }
            else
            {
                blinkLED(QUICK_BLINK_DURATION, 5);
                blinkLED(LONG_BLINK_DURATION, 1);
            }
        }
    }
}