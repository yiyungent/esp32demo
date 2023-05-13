#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define LED_BUILTIN 4

const char *wifiSsid = "ffjfifjfjfjfjfjf";
const char *wifiPassword = "1478963ZXCV";
unsigned long previousMillis = 0;
unsigned long interval = 30000;

const char *mqttServer = "your_MQTT_server";
const int mqttPort = 1883;
const char *mqttUser = "your_MQTT_user";
const char *mqttPassword = "your_MQTT_password";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

void LEDUp()
{
  digitalWrite(LED_BUILTIN, HIGH);
}

void LEDDown()
{
  digitalWrite(LED_BUILTIN, LOW);
}

void initWiFi()
{
  LEDUp();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  LEDDown();
  Serial.println("Connected to WiFi");
  Serial.print("WiFi.localIP: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi.RRSI: ");
  Serial.println(WiFi.RSSI());
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");
  
  // 设置 GPIO 4 的模式为: 输出模式
  pinMode(LED_BUILTIN, OUTPUT);

  initWiFi();
}

void loop()
{
  Serial.println("loop");

#pragma region 定时重连 WiFi
  unsigned long currentMillis = millis();
  // 定时重连
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    LEDUp();
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    if (WiFi.status() == WL_CONNECTED)
    {
      LEDDown();
      Serial.print("WiFi.localIP: ");
      Serial.println(WiFi.localIP());
    }
    previousMillis = currentMillis;
  }
#pragma endregion


}