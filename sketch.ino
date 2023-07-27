#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>

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
#define Button_02_pin 39
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

#define distance_lock 3             // cm
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

  // COPY TỪ ĐÂY
  Servo servo;
  bool isPressing_01 = false, isPressing_02 = false;
  bool Pass_sucess = true, isClose = true, isLock = false;
  int pos = 0;

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
    // set up other devices down here
    servo.attach(servo_pin, 500, 2400);
    servo.write(pos);
    pinMode(Button_01_pin, INPUT);
    pinMode(Button_02_pin, INPUT);
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
    if (isLock)
    {
      analogWrite(PIN_RED_02, 200);
      analogWrite(PIN_GREEN_02, 0);
      analogWrite(PIN_BLUE_02, 0);
    }
    else
    {
      analogWrite(PIN_GREEN_02, 200);
      analogWrite(PIN_RED_02, 0);
      analogWrite(PIN_BLUE_02, 0);
    }
  }
  void lock_unlock()
  {
    int button_01 = digitalRead(Button_01_pin);
    int button_02 = digitalRead(Button_02_pin);
    BUTTON_02(button_02);
    BUTTON_01(button_01);
    LED_RBG_BACK();
  }
  void BUTTON_01(int button_01)
  {
    if (Pass_sucess)
    {
      int distance = Ultrasonic();
      if (distance > distance_lock)
      {
        // gửi tín hiệu về serve
        Serial.println("Please close the door");

        isClose = false;
      }
      // LOCK THE DOOR
      if (button_01 == HIGH && pos == 0 && isPressing_01 == false && isClose == true)
      {
        isLock = true;
        pos = 180;
        servo.write(pos);
      }
      // UNLOCK THE DOOR
      else if (button_01 == HIGH && pos == 180 && isPressing_01 == false)
      {
        isLock = false;
        pos = 0;
        servo.write(pos);
      }
      if (button_01 == HIGH)
        isPressing_01 = true;
      if (button_01 == LOW)
        isPressing_01 = false;
    }
  }

  void BUTTON_02(int button_02)
  {
    int distance = Ultrasonic();
    if (distance > distance_lock)
    {
      // gửi tín hiệu về
      Serial.println("Please close the door");
      isClose = false;
    }
    // LOCK THE DOOR
    if (button_02 == HIGH && pos == 0 && isPressing_02 == false && isClose == true)
    {
      pos = 180;
      isLock = true;
      servo.write(pos);
    }
    // UNLOCK THE DOOR
    else if (button_02 == HIGH && pos == 180 && isPressing_02 == false)
    {
      pos = 0;
      isLock = false;
      servo.write(pos);
    }
    if (button_02 == HIGH)
      isPressing_02 = true;
    if (button_02 == LOW)
      isPressing_02 = false;
  }

  void loop()
  {
    // always check if client is disconnected, reconnect
    if (!client.connected())
      reconnect();

    client.loop(); // giúp giữ kết nối với server và để hàm callback được gọi

    // devices
    lock_unlock();
  }
