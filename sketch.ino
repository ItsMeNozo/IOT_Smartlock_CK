#include "PubSubClient.h" // MQTT
#include "WiFi.h"		  // for ESP32
#include <ESP32Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// simulate connecting to internet through WIFI using ESP32
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "broker.hivemq.com"; // Thingspeak MQTT broker
int port = 1883;

// sub/pub
const char *sub_lock = "web/lock";		// currently sub to this topic so we'll know user from the web trying to turn off/on lock switch
const char *sub_pass = "21127119/pass"; // currently sub to this topic so we'll know user from the web trying to turn off/on lock switch

const char *pub_lock = "device/lock_stat";
const char *pub_door_close = "device/door_not_closed";
const char *pub_device_on = "21127119/device_activated";

const char *Count_down_time = "time/time_out";
// wifi setup through TCP/IP
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#define ROW_NUM 4
#define COLUMN_NUM 4
// buzzer
#define Buzzer_pin 13
// button
#define outerButtonPin 16
#define innerButtonPin 39
// light
#define light_sensor 35
// Ultrasonic Distance Sensor
#define trig_pin 2
#define echo_pin 36
#define servo_pin 15
// RBG 01
#define PIN_RED_01 4	// GIOP26
#define PIN_GREEN_01 12 // GIOP33
#define PIN_BLUE_01 14	// GIOP25
// RBG 02
#define PIN_RED_02 27	// GIOp13
#define PIN_GREEN_02 26 // GIOP12
#define PIN_BLUE_02 25	// GIOP14
// lcd 16x2 i2c

#define distance_lock 3				// cm
LiquidCrystal_I2C lcd(0x27, 16, 2); // lcd

// keypad
char keys[ROW_NUM][COLUMN_NUM] = {
	{'1', '2', '3', 'A'},
	{'4', '5', '6', 'B'},
	{'7', '8', '9', 'C'},
	{'*', '0', '#', 'D'}};
byte pin_rows[ROW_NUM] = {17, 5, 3, 23};
byte pin_column[COLUMN_NUM] = {33, 32, 18, 19};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

Servo servo;
bool Pass_success = true;
bool isOuterPressing = false, isInnerPressing = false;
bool IsSetCountdown = false;
int degree = 0, Count_down = 0, time_out = 60 * 1000;
String lockedStr = "🔒 Locked";
String unlockedStr = "🔓 Unlocked";
String correctPassword = "N";

// Thingspeak
const char *host = "api.thingspeak.com";
const int portTS = 80;
// const char* requestDangerSent = "/update?api_key=RUK1GD4GABD5TQEN&field1=3";
const char *request = "/update?api_key=RUK1GD4GABD5TQEN&field1=";

void sendRequest(const char *request)
{
	Serial.println(request);
	WiFiClient client;
	while (!client.connect(host, portTS))
	{
		Serial.println("connection fail");
		delay(1000);
	}
	client.print(String("GET ") + request + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
	delay(500);

	while (client.available())
	{
		String line = client.readStringUntil('\r');
		Serial.print(line);
	}
}

void setup()
{
	Serial.begin(9600);

	// lcd
	lcd.init();
	lcd.backlight();

	// wifi
	setupWifi();
	mqttClient.setServer(mqtt_server, port);
	mqttClient.setCallback(callback);
	mqttClient.setKeepAlive(90); // if client does not send any data to broker within 60-sec interval, disconnect

	// set up other devices down here
	servo.attach(servo_pin, 500, 2400);
	servo.write(degree);
	pinMode(outerButtonPin, INPUT);
	pinMode(innerButtonPin, INPUT);
	// ultrasonic
	pinMode(trig_pin, OUTPUT);
	pinMode(echo_pin, INPUT);
	// led
	pinMode(PIN_RED_02, OUTPUT);
	pinMode(PIN_GREEN_02, OUTPUT);
	pinMode(PIN_BLUE_02, OUTPUT);
	// buzzer
	pinMode(Buzzer_pin, OUTPUT);

	// led for not locked
	analogWrite(PIN_GREEN_02, 0);
	analogWrite(PIN_RED_02, 255);
	analogWrite(PIN_BLUE_02, 0);

	// auto lighting for keypad
	pinMode(35, INPUT);
	pinMode(PIN_RED_01, OUTPUT);
	pinMode(PIN_GREEN_01, OUTPUT);
	pinMode(PIN_BLUE_01, OUTPUT);

	// mqtt
	mqttConnect();

	// pub device activated
	String payload = "Device on";
	mqttClient.publish(pub_device_on, payload.c_str());
}

int Ultrasonic()
{
	digitalWrite(trig_pin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trig_pin, LOW);
	int duration_us = pulseIn(echo_pin, HIGH);
	int distance_cm = 0.017 * duration_us;
	return distance_cm;
}

void lock()
{
	degree = 180;
	servo.write(degree);

	// led green
	analogWrite(PIN_RED_02, 0);
	analogWrite(PIN_GREEN_02, 255);

	mqttClient.publish(pub_lock, lockedStr.c_str());

	String requestLockOn = String(request) + "1";
	sendRequest(requestLockOn.c_str());
}

void unlock()
{
	degree = 0;
	servo.write(degree);

	// led red
	analogWrite(PIN_GREEN_02, 0);
	analogWrite(PIN_RED_02, 255);

	mqttClient.publish(pub_lock, unlockedStr.c_str());

	String requestLockOff = String(request) + "2";

	sendRequest(requestLockOff.c_str());
}

bool isClosed()
{
	int distance = Ultrasonic();
	return distance <= distance_lock;
}
bool isLocked()
{
	return degree == 180;
}

String userInput = "";
int unlockFailCounts = 0;
bool disableKeypad = false;
unsigned long startDisable;
bool isDangerMode = false;
int buzzerPin = 12, buzzerTone = 500;
bool isReplied = false;

void initAuthen()
{
	userInput = "";
	unlockFailCounts = 0;
	disableKeypad = false;
	isDangerMode = false;
}

bool isPasswordCorrect()
{
	Serial.println(userInput);
	// get the correct password from the cloud
	return userInput == correctPassword;
}

void turnOnDangerMode()
{
	isDangerMode = true;
	String payload = "Danger mode options";
	mqttClient.publish("21127089/dangerModeOpts", payload.c_str());
}
void turnOffDangerMode()
{
	isDangerMode = false;
	noTone(Buzzer_pin);
}
void authenBeforeUnlock()
{
	lcd.clear();

	if (userInput.length() == 0)
		return;

	if (isPasswordCorrect())
	{
		lcd.print("Access granted");
		unlock();
		initAuthen();
	}
	else
	{
		lcd.print("Access denied");
		unlockFailCounts += 1;
		Serial.print("Unlock fail counts: ");
		Serial.println(unlockFailCounts);
	}
	delay(1000);
	lcd.clear();
	userInput = "";
}

void handleKeypad()
{
	if (isDangerMode)
	{
		Serial.println("danger mode is on");
		// buzzer for dangermode
		tone(Buzzer_pin, buzzerTone);
		buzzerTone++;
		if (buzzerTone == 510)
			buzzerTone = 500;
		return;
	}
	if (disableKeypad)
	{
		Serial.println("keypad is disabled");
		if (!isReplied && millis() - startDisable >= 10 * 1000)
		{
			turnOnDangerMode();
		}
		return;
	}
	if (unlockFailCounts == 3)
	{
		Serial.println("you failed to unlock 3 times");
		disableKeypad = true;
		isReplied = false;
		startDisable = millis();
		// handleTelegramNotification()
		String payload = "Ayo you unlock too many times!";
		mqttClient.publish("21127089/doorAlert", payload.c_str());
		Serial.println("Just sent alert to node red");
		// write to cloud
		String requestDanger = String(request) + "3";
		sendRequest(requestDanger.c_str());
	}
	if (!isLocked())
	{
		// Serial.println("The door is unlocked so the keypad is disabled");
		return;
	}

	char key = keypad.getKey();
	if (key != NO_KEY)
	{
		if (key == 'D')
		{
			lcd.clear();
			userInput = "";
		}
		else if (key == 'A')
		{
			authenBeforeUnlock();
		}
		else
		{
			lcd.print(key);
			Serial.print(key);
			userInput += key;
		}
	}
}

// button outside the door
void handleOuterButton()
{
	// LOCK THE DOOR
	if (!isLocked())
	{
		if (isClosed())
			lock();
		else
		{
			// lcd displays Please close the door
			lcd.clear();
			lcd.println("Close door first");
		}
	}
	else
	{
		authenBeforeUnlock();
	}
}
// button inside the door
void handleInnerButton()
{
	// LOCK THE DOOR
	if (!isLocked())
	{
		if (isClosed())
			lock();
	}
	else
	{
		// UNLOCK THE DOOR
		unlock();
	}
}
void set_time()
{
	if (!IsSetCountdown)
	{
		Count_down = millis();
		IsSetCountdown = true;
	}
}
void schedule_auto()
{
	if (isClosed())
	{
		if (!isLocked())
		{
			set_time();
			if (millis() - Count_down > time_out)
				handleInnerButton();
		}
		else
			IsSetCountdown = false;
	}
	else
		IsSetCountdown = false;
}
void lock_unlock()
{
	int outerButtonState = digitalRead(outerButtonPin);
	int innerButtonState = digitalRead(innerButtonPin);
	if (outerButtonState && !isOuterPressing)
		handleOuterButton();
	if (innerButtonState && !isInnerPressing)
		handleInnerButton();
	isOuterPressing = outerButtonState;
	isInnerPressing = innerButtonState;
	// int switchState = data.switchState;
	// handleOuterButton(switchState, true);
}
bool kpLightOn = false;
void autoKeypadLight() {
	if (analogRead(35) > 1700) { // turn on
		if (!kpLightOn) {
			analogWrite(PIN_GREEN_01, 255);
			analogWrite(PIN_RED_01, 255);
			analogWrite(PIN_BLUE_01, 255);
			kpLightOn = true;
		}
	} else if (kpLightOn) { // turn off
		analogWrite(PIN_GREEN_01, 0);
		analogWrite(PIN_RED_01, 0);
		analogWrite(PIN_BLUE_01, 0);
		kpLightOn = false;
	}
}

// subscribe callback
void callback(char *topic, byte *payload, unsigned int length)
{
	Serial.print("Topic arrived: ");
	Serial.println(topic);

	String data = "";
	for (int i = 0; i < length; ++i)
	{
		data += (char)payload[i];
	}

	Serial.print("Message: ");
	Serial.println(data);

	if (String(topic) == sub_lock)
	{
		if (data == "true")
		{
			if (!isLocked())
			{
				if (isClosed())
				{
					lock();
				}
				else
				{
					String payload = "Cant lock because door is not closed!";
					mqttClient.publish(pub_door_close, payload.c_str());
				}
			}
		}
		else
		{
			if (isLocked())
				unlock();
		}
	}

	else if (String(topic) == Count_down_time)
	{
		time_out = atoi(data.c_str()) * 60000;
		Serial.println(time_out);
	}

	else if (String(topic) == sub_pass)
	{
		correctPassword = data;

		Serial.println("----------------");
		if (String(topic) == "21127089/alertReply")
		{
			if (data == "It's me")
			{
				isReplied = true;
				initAuthen();
			}
			else if (data == "It's not me")
			{
				isReplied = true;
				turnOnDangerMode();
			}
			else if (data == "Turn off danger mode")
			{
				turnOffDangerMode();
				initAuthen();
			}
		}
	}
}

void mqttConnect()
{
	// if client is connected, stop reconnecting
	while (!mqttClient.connected())
	{
		Serial.print("Attempting MQTT connection...");
		// Create a random client ID as a unique identifier to avoid conflicts with other clients.
		String clientId = "ESP32Client-";
		clientId += String(random(0xffff), HEX);

		if (mqttClient.connect(clientId.c_str()))
		{

			Serial.println(" connected");
			lcd.clear();
			lcd.print("connected");
			delay(1000);
			lcd.clear();

			// once connected, do publish/subscribe
			mqttClient.subscribe(sub_lock);
			mqttClient.subscribe("21127089/alertReply");
			mqttClient.subscribe(sub_pass);
			mqttClient.subscribe(Count_down_time);
		}
		else
		{
			Serial.print("failed");
			Serial.print(mqttClient.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}

	// mqtt init: check for lock status to publish onto web -> initially, on web doesnt show lock status
	String strLock = (isLocked() ? lockedStr : unlockedStr);

	mqttClient.publish(pub_lock, strLock.c_str());
}
void wifiConnect()
{
	WiFi.begin(ssid, password);

	// wait until wifi is connected
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	Serial.println("WiFi connected");

	lcd.print("Connected");
}
void setupWifi()
{
	Serial.print("Connecting to ");
	Serial.print(ssid);

	lcd.setCursor(0, 0);
	lcd.print("Connecting to ");
	lcd.setCursor(0, 1);
	lcd.print("WIFI. ");

	wifiConnect();
}
void loop()
{
	// always check if client is disconnected, reconnect
	if (!mqttClient.connected())
		mqttConnect();

	mqttClient.loop(); // giúp giữ kết nối với server và để hàm callback được gọi

	// devices
	lock_unlock();

	//
	handleKeypad();

	schedule_auto();

	autoKeypadLight();
	delay(100);
}
