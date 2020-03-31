/*
 Name:		IoTLapTimer.ino
 Created:	3/20/2020 2:10:44 PM
 Author:	johnt
*/
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "NetworkConfiguration.h"

const NetworkConfiguration _netConfig = NetworkConfiguration();
const int _quickBlinkDuration = 150;
const int _longBlinkDuration = 750;
const int _blinkInterval = 250;
const String _registrationClosed = "Registration closed. Max participants reached.";

enum TimerState {
	NOT_REGISTERED = 0,
	REGISTERED,
	START_RACE,
	RACE_IN_PROGRESS,
	RACE_FINISH
};

int _id;
bool _ledOn;
TimerState _timerState;
ESP8266WiFiMulti _wifiMulti;
IPAddress _ipAddress;

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
//		2.3 - Registration closed (max number of participants has been reached): 5 quick blinks, 1 long blink

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
		blinkLED(_quickBlinkDuration, 2);
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
		blinkLED(_quickBlinkDuration, _quickBlinkDuration, INT_MAX);
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
	blinkLED(onDuration, _blinkInterval, blinkCount);
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
		break;
		//REGISTERED
		//START_RACE
		//RACE_IN_PROGRESS
		//RACE_FINISH
	}
}

void registerTimer()
{
	WiFiClient wifiClient;
	HTTPClient httpClient;

	Serial.print("[HTTP] begin...\n");
	String registerUrl = _netConfig.apiPrefix;
	registerUrl += "/Register/";

	Serial.println("_ipAddress: ");
	Serial.println(_ipAddress);

	registerUrl += _ipAddress.toString();
	Serial.println("registerUrl: ");
	Serial.println(registerUrl);

	if (httpClient.begin(wifiClient, registerUrl)) {
		int httpCode = httpClient.GET();

		if (httpCode > 0) { /// HTTP client errors are negative
			Serial.printf("[HTTP] GET... code: %d\n", httpCode);
			String responseBody = httpClient.getString();
			Serial.println("response Body: ");
			Serial.println(responseBody);

			if (httpCode == HTTP_CODE_OK) {
				_id = responseBody.toInt();
				_timerState = REGISTERED;
			}
			else if (responseBody.indexOf("Max participants reached") > 0)
			{
				blinkLED(_quickBlinkDuration, 3);
			}
		}
		else
		{
			Serial.printf("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
		}

		httpClient.end();
	}

	if (_timerState == NOT_REGISTERED)
	{
		blinkLED(_quickBlinkDuration, 2);
		blinkLED(_longBlinkDuration, 1);
	}
}