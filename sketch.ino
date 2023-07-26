#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include "WiFi.h" // for ESP32
#include "PubSubClient.h" // MQTT

// simulate connecting to internet through WIFI using ESP32
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "mqtt3.thingspeak.com"; // Thingspeak MQTT broker
const char* pTopic = "web/switch"; 

// wifi setup through TCP/IP
WiFiClient espClient; 
PubSubClient client(espClient); 


#define ROW_NUM     4 
#define COLUMN_NUM  4
//buzzer
#define Buzzer_pin 23
//button
#define Button_01_pin 1
#define Button_02_pin 3
//light
#define light_sensor 36
// Ultrasonic Distance Sensor
#define trig_pin 4
#define echo_pin 2
#define servo_pin 15
//RBG 01
#define PIN_RED_01   26 // GIOP26
#define PIN_GREEN_01 33 // GIOP33
#define PIN_BLUE_01 25 // GIOP25
// RBG 02
#define PIN_RED_02 13 // GIOp13
#define PIN_GREEN_02 12 // GIOP12
#define PIN_BLUE_02 14 // GIOP14
// lcd 16x2 i2c
LiquidCrystal_I2C lcd(0x27,16, 2);  // lcd

// keypad
char keys[ROW_NUM][COLUMN_NUM] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte pin_rows[ROW_NUM] = {17,5,18,19};
byte pin_column[COLUMN_NUM] = {33,32,35,34};
Keypad keypad = Keypad( makeKeymap(keys),  pin_rows,pin_column,ROW_NUM,COLUMN_NUM);


void setupWifi(){
  Serial.println("Connecting to ");
  Serial.println(ssid);

  lcd.
}
// test
void setup() {
  Serial.begin(9600); 
  lcd.init();
  lcd.backlight();

  // set up other devices down here

}

void loop() {
  
}



