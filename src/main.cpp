#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>
const char *id = "ffjfifjfjfjfjfjf"; // 定义两个字符串指针常量
const char *psw = "1478963ZXCV";

#define LED_BUILTIN 4

void setup()
{
  Serial.begin(115200);

  //  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // 设置GPIO 2的模式为：输出模式

  WiFi.begin(id, psw);
  while (WiFi.status() != WL_CONNECTED)
  { // 未连接上
    delay(500);
    Serial.println("正在连接...");
  }
  Serial.println("连接成功！");

  // put your setup code here, to run once:
  Serial.println("setup");
}

void loop()
{
  // put your main code here, to run repeatedly:
  Serial.println("Hello ");

  //  digitalWrite(LED_BUILTIN, HIGH); //把GPIO 2设置为高电平，让灯熄灭。
  //  delay(200);//阻塞延时200MS
  //  digitalWrite(LED_BUILTIN, LOW); //把GPIO 2设置为高电平，让灯亮起。
  //  Serial.println("LED灯亮。");//串口输出内容
  //  delay(200);//阻塞延时200MS

  HTTPClient http;
  http.begin("https://visitor_badge.deta.dev/?pageID=github.Lete114"); // Specify destination for HTTP request
  // start get
  int http_code = http.GET();
  Serial.println(http_code);
  // handle http code
  if (http_code != HTTP_CODE_OK)
  {
    // get fail.
    Serial.printf("GET fail, http code is %s\n", http.errorToString(http_code).c_str());
    return;
  }

  // http response
  String response = http.getString();
  Serial.printf("response:[%s]\n", response.c_str());
  // close http
  http.end();
}