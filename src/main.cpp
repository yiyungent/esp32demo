/**
 * Blink
 *
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */
#include "Arduino.h"

// Set LED_BUILTIN if it is not defined by Arduino framework
// #define LED_BUILTIN 13

void setup()
{
  // initialize LED digital pin as an output.
  // pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(2,OUTPUT);//设置GPIO 2的模式为：输出模式
  Serial.begin(4800);//设置串口输出波特率
  Serial.println("setup");
}

void loop()
{
  // digitalWrite(2,HIGH);//把GPIO 2设置为高电平，让灯熄灭。
  // while (true)
  // {
  //   Serial.println("Hello ");
  // }
  
  Serial.println("LED mie");//串口输出内容
  // delay(200);//阻塞延时200MS
  // digitalWrite(2,LOW);//把GPIO 2设置为高电平，让灯亮起。
  // Serial.println("LED灯亮。");//串口输出内容
  // delay(200);//阻塞延时200MS

  
}