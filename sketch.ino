#include "PubSubClient.h" // MQTT
#include "WiFi.h"					// for ESP32
#include <ESP32Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// simulate connecting to internet through WIFI using ESP32
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "broker.hivemq.com"; // Thingspeak MQTT broker
int port = 1883;

// sub/pub
const char *sub_lock = "web/lock"; // currently sub to this topic so we'll know user from the web trying to turn off/on lock switch
const char *pub_lock = "web/lock_stat";
// wifi setup through TCP/IP
WiFiClient wifiClient;
PubSubClient client(wifiClient);

#define ROW_NUM 4
#define COLUMN_NUM 4
// buzzer
#define Buzzer_pin 23
// button
#define outerButtonPin 16
#define innerButtonPin 39
// light
#define light_sensor 36
// Ultrasonic Distance Sensor
#define trig_pin 4
#define echo_pin 2
#define servo_pin 15
// RBG 01
#define PIN_RED_01 26		// GIOP26
#define PIN_GREEN_01 33 // GIOP33
#define PIN_BLUE_01 25	// GIOP25
// RBG 02
#define PIN_RED_02 13		// GIOp13
#define PIN_GREEN_02 12 // GIOP12
#define PIN_BLUE_02 14	// GIOP14
// lcd 16x2 i2c

#define distance_lock 3							// cm
LiquidCrystal_I2C lcd(0x27, 16, 2); // lcd

// keypad
char keys[ROW_NUM][COLUMN_NUM] = {
		{'1', '2', '3', 'A'},
		{'4', '5', '6', 'B'},
		{'7', '8', '9', 'C'},
		{'*', '0', '#', 'D'}};
byte pin_rows[ROW_NUM] = {17, 5, 18, 19};
byte pin_column[COLUMN_NUM] = {33, 32, 35, 34};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

Servo servo;
bool isPressing_01 = false, isPressing_02 = false;
bool Pass_sucess = true;
int degree = 0;

void setup()
{
	Serial.begin(9600);

	// lcd
	lcd.init();
	lcd.backlight();

	// wifi
	setupWifi();
	client.setServer(mqtt_server, port);
	client.setCallback(callback);
	client.setKeepAlive(90); // if client does not send any data to broker within 60-sec interval, disconnect

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
void LED_RBG_BACK()
{
	if (isLocked())
	{
		analogWrite(PIN_RED_02, 0);
		analogWrite(PIN_GREEN_02, 255);
		analogWrite(PIN_BLUE_02, 0);
	}
	else
	{
		analogWrite(PIN_GREEN_02, 0);
		analogWrite(PIN_RED_02, 255);
		analogWrite(PIN_BLUE_02, 0);
	}
}
void lock_unlock()
{
	int outerButtonState = digitalRead(outerButtonPin);
	int innerButtonState = digitalRead(innerButtonPin);
	handleInnterButton(innerButtonState);
	handleOuterButton(outerButtonState);
	LED_RBG_BACK();
	// int switchState = data.switchState;
	// handleOuterButton(switchState, true);
}

void lock()
{
	degree = 180;
	servo.write(degree);
}

void unlock()
{
	degree = 0;
	servo.write(degree);
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
void remoteButton()
{
	if (switched == 1)
		BUTTON_01(buttonSt, true);
}
// button outside the door
void handleOuterButton(int buttonState, bool openFromWeb)
{
	// LOCK THE DOOR
	if (isLocked())
	{
		if (buttonState == HIGH && isPressing_01 == false)
		{
			if (isClosed())
				lock();
			else
			{
				if (openFromWeb)
				{
					// send back to channel for node red to display notification
				}
				else
				{
					// lcd displays Please close the door
					Serial.println("Please close the door");
				}
			}
		}
	}
	else
	{
		// UNLOCK THE DOOR
		if (Pass_sucess)
		{
			// Access accepted
			if (buttonState == HIGH && isPressing_01 == false)
			{
				unlock();
			}
		}
		else
		{
			// LCD displays Access denied
		}
	}

	if (buttonState == HIGH)
		isPressing_01 = true;
	if (buttonState == LOW)
		isPressing_01 = false;
}

// button inside the door
void handleInnerButton(int buttonState)
{
	// LOCK THE DOOR
	if (isLocked())
	{
		if (buttonState == HIGH && isPressing_01 == false && isClosed())
		{
			lock();
		}
	}
	else
	{
		// UNLOCK THE DOOR
		if (buttonState == HIGH && isPressing_01 == false)
		{

			unlock();
		}
	}

	if (buttonState == HIGH)
		isPressing_01 = true;
	if (buttonState == LOW)
		isPressing_01 = false;
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
			servo.write(0);
			Serial.println("Lock on");
		}
		else
		{
			servo.write(90);

			Serial.println("Lock off");
		}
	}
	Serial.print("----------------");
}

void reconnect()
{
	// if client is connected, stop reconnecting
	while (!client.connected())
	{
		Serial.print("Attempting MQTT connection...");
		// Create a random client ID as a unique identifier to avoid conflicts with other clients.
		String clientId = "ESP32Client-";
		clientId += String(random(0xffff), HEX);

		if (client.connect(clientId.c_str()))
		{
			Serial.println(" connected");
			lcd.clear();
			lcd.print("connected");

			// once connected, do publish/subscribe
			client.subscribe(sub_lock);
		}
		else
		{
			Serial.print("failed");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
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
	if (!client.connected())
		reconnect();

	client.loop(); // giúp giữ kết nối với server và để hàm callback được gọi

	// devices
	lock_unlock();

	delay(3000);
}
