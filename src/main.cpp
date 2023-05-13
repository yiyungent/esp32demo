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

void LEDFlash(int count)
{
  int ledStatus = digitalRead(LED_BUILTIN);
  for (int i = 0; i < count; i++)
  {
    LEDUp();
    // 阻塞延时 200ms
    delay(200);
    LEDDown();
    delay(200);
  }
  digitalWrite(LED_BUILTIN, ledStatus);
}

/// @brief WiFi 连接事件
/// @param event
/// @param info
void WiFiStationConnected(arduino_event_id_t event, arduino_event_info_t info)
{
  Serial.println("Connected to WiFi(AP) successfully!");
  // LEDFlash(3);
  LEDFlash(5);
  Serial.println("Connected to WiFi");
  Serial.print("WiFi.localIP: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi.RRSI: ");
  Serial.println(WiFi.RSSI());
}

/// @brief WiFi 断开连接事件
/// @param event
/// @param info
void WiFiStationDisconnected(arduino_event_id_t event, arduino_event_info_t info)
{
  Serial.println("Disconnected from WiFi access point");
  // 注意: 断开时不要闪灯, 因为 loop 内重连会不停断开
  // LEDFlash(5);
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
}

/// @brief 启动时初始化 WiFi
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(wifiSsid, wifiPassword);
  Serial.print("Connecting to WiFi ..");
  int i = 0;
  // 启动时 尝试 5 次连接 WiFi, 若超过次数则放弃
  while (WiFi.status() != WL_CONNECTED && i < 5)
  {
    Serial.print('.');
    delay(1000);
    i++;
  }
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
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
#pragma endregion

  // TODO: 其它业务
}