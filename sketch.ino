#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include "WiFi.h"         // for ESP32
#include "PubSubClient.h" // MQTT

// simulate connecting to internet through WIFI using ESP32
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "broker.hivemq.com"; // Thingspeak MQTT broker
int port = 1883;

// sub/pub
const char *sub_lock = "web/lock"; // currently sub to this topic so we'll know user from the web trying to turn off/on lock switch

// wifi setup through TCP/IP
WiFiClient espClient;
PubSubClient client(espClient);

#define ROW_NUM 4
#define COLUMN_NUM 4
// buzzer
#define Buzzer_pin 23
// button
#define Button_01_pin 16
#define Button_02_pin 3
// light
#define light_sensor 36
// Ultrasonic Distance Sensor
#define trig_pin 4
#define echo_pin 2
#define servo_pin 15
// RBG 01
#define PIN_RED_01 26   // GIOP26
#define PIN_GREEN_01 33 // GIOP33
#define PIN_BLUE_01 25  // GIOP25
// RBG 02
#define PIN_RED_02 13   // GIOp13
#define PIN_GREEN_02 12 // GIOP12
#define PIN_BLUE_02 14  // GIOP14
// lcd 16x2 i2c
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
      Serial.println("Lock on");
    else
      Serial.println("Lock off");
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

void setupWifi()
{
  Serial.println("Connecting to ");
  Serial.println(ssid);

  lcd.setCursor(0, 0);
  lcd.print("Connecting to ");
  lcd.setCursor(0, 1);
  lcd.print("WIFI. ");

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

// test
void setup()
{
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  setupWifi();
  client.setServer(mqtt_server, port);
  client.setCallback(callback);
  // set up other devices down here
}

void loop()
{
  // always check if client is disconnected, reconnect
  if (!client.connected())
    reconnect();

  client.loop(); // like socket listen: listening to notifications (subscription)
}
