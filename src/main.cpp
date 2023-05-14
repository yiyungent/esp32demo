#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "vars.h"

#pragma region camera

/*
  巴法云物联网平台 https://bemfa.com

  分辨率默认配置：config.frame_size = FRAMESIZE_UXGA;
  其他配置：
  FRAMESIZE_UXGA （1600 x 1200）
  FRAMESIZE_QVGA （320 x 240）
  FRAMESIZE_CIF （352 x 288）
  FRAMESIZE_VGA （640 x 480）
  FRAMESIZE_SVGA （800 x 600）
  FRAMESIZE_XGA （1024 x 768）
  FRAMESIZE_SXGA （1280 x 1024）

  config.jpeg_quality = 10;（10-63）越小照片质量最好
  数字越小表示质量越高，但是，如果图像质量的数字过低，尤其是在高分辨率时，可能会导致ESP32-CAM崩溃

  支持发布订阅模式，当图片上传时，订阅端会自动获取图片url地址，可做图片识别，人脸识别，图像分析
*/

#include "esp_camera.h"
#include "esp_http_client.h"

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
// #define CAMERA_MODEL_WROVER_KIT // Has PSRAM
// #define CAMERA_MODEL_ESP_EYE // Has PSRAM
// #define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
// #define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
// #define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
// #define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
// #define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
// #define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//  ** Espressif Internal Boards **
// #define CAMERA_MODEL_ESP32_CAM_BOARD
// #define CAMERA_MODEL_ESP32S2_CAM_BOARD
// #define CAMERA_MODEL_ESP32S3_CAM_LCD

#include "camera_pins.h"

int capture_interval = CAPTURE_INTERVAL;       // 默认5分钟上传一次，可更改（本项目是自动上传，如需条件触发上传，在需要上传的时候，调用take_send_photo()即可）
const char *bemfa_uid = BEMFA_UID;     // 用户私钥，巴法云控制台获取
const char *bemfa_topic = BEMFA_TOPIC; // 主题名字，可在控制台新建
const char *wechatMsg = "";            // 如果不为空，会推送到微信，可随意修改，修改为自己需要发送的消息
const char *wecomMsg = "";             // 如果不为空，会推送到企业微信，推送到企业微信的消息，可随意修改，修改为自己需要发送的消息
const char *urlPath = "";              // 如果不为空，会生成自定义图片链接，自定义图片上传后返回的图片url，url前一部分为巴法云域名，第二部分：私钥+主题名的md5值，第三部分为设置的图片链接值。
bool flashRequired = false;            // 闪光灯，true是打开闪光灯
const int brightLED = 4;               // 默认闪光灯引脚

/********************************************************/

#pragma region mail
#include <ESP_Mail_Client.h>

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);
#pragma endregion

const char *bemfa_post_url = BEMFA_POST_URL; // 默认上传地址
static String httpResponseString;            // 接收服务器返回信息
bool internet_connected = false;
long current_millis;
long last_capture_millis = 0;

// Bright LED (Flash)
const int ledFreq = 50;       // PWM settings
const int ledChannel = 15;    // camera uses timer1
const int ledRresolution = 8; // resolution (8 = from 0 to 255)
                              // change illumination LED brightness
void brightLed(byte ledBrightness)
{
  ledcWrite(ledChannel, ledBrightness); // change LED brightness (0 - 255)
  Serial.println("Brightness changed to " + String(ledBrightness));
}

// ----------------------------------------------------------------
//       set up PWM for the illumination LED (flash)
// ----------------------------------------------------------------
// note: I am not sure PWM is very reliable on the esp32cam - requires more testing
void setupFlashPWM()
{
  ledcSetup(ledChannel, ledFreq, ledRresolution);
  ledcAttachPin(brightLED, ledChannel);
  brightLed(0);
}

#pragma endregion

#pragma region base
#define LED_BUILTIN 4

const char *wifiSsid = WIFI_SSID;
const char *wifiPassword = WIFI_PASSWORD;
unsigned long previousMillis = 0;
unsigned long interval = 30000;

const char *mqttServer = "your_MQTT_server";
const int mqttPort = 1883;
const char *mqttUser = "your_MQTT_user";
const char *mqttPassword = "your_MQTT_password";

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
#pragma endregion

#pragma region LED

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

#pragma endregion

#pragma region WiFi
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

#pragma endregion

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");

  // 设置 GPIO 4 的模式为: 输出模式
  pinMode(LED_BUILTIN, OUTPUT);

  initWiFi();

#pragma region camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  // config.pin_sscb_sda = SIOD_GPIO_NUM; // deprecated
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  // config.pin_sscb_scl = SIOC_GPIO_NUM; // deprecated
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG)
  {
    if (psramFound())
    {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }
  else
  {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID)
  {
    s->set_vflip(s, 1);       // flip it back
    s->set_brightness(s, 1);  // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif
  setupFlashPWM(); // configure PWM for the illumination LED
#pragma endregion

#pragma region mail
  /*  Set the network reconnection option */
  MailClient.networkReconnect(true);

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);
#pragma endregion
}

#pragma region camera
/********http请求处理函数*********/
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  if (evt->event_id == HTTP_EVENT_ON_DATA)
  {
    httpResponseString.concat((char *)evt->data);
  }
  return ESP_OK;
}

static esp_err_t take_send_photo()
{
  if (flashRequired)
  {
    brightLed(255);
    delay(300);
  }
  Serial.println("take_send_photo...");
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  fb = esp_camera_fb_get();
  if (flashRequired)
    brightLed(0); // change LED brightness back to previous state

  if (!fb)
  {
    Serial.println("Camera capture failed...");
    return ESP_FAIL;
  }

#pragma region http send 巴法云
  httpResponseString = "";
  esp_http_client_handle_t http_client;
  esp_http_client_config_t config_client = {0};
  config_client.url = bemfa_post_url;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;
  http_client = esp_http_client_init(&config_client);
  esp_http_client_set_post_field(http_client, (const char *)fb->buf, fb->len); // 设置http发送的内容和长度
  esp_http_client_set_header(http_client, "Content-Type", "image/jpg");        // 设置http头部字段
  esp_http_client_set_header(http_client, "Authorization", bemfa_uid);         // 设置http头部字段
  esp_http_client_set_header(http_client, "Authtopic", bemfa_topic);           // 设置http头部字段
  esp_http_client_set_header(http_client, "wechatmsg", wechatMsg);             // 设置http头部字段
  esp_http_client_set_header(http_client, "wecommsg", wecomMsg);               // 设置http头部字段
  esp_http_client_set_header(http_client, "picpath", urlPath);                 // 设置http头部字段
  esp_err_t err = esp_http_client_perform(http_client);                        // 发送http请求
  if (err == ESP_OK)
  {
    Serial.println(httpResponseString); // 打印获取的URL
    // json数据解析
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, httpResponseString);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    String url = doc["url"];
    Serial.println(url); // 打印获取的URL
  }

  // Serial.println("Taking picture END");
  // esp_camera_fb_return(fb);
  esp_http_client_cleanup(http_client);
#pragma endregion

#pragma region send mail
  /* Declare the Session_Config for user defined session credentials */
  Session_Config config;

  /* Set the session config */
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;

  /** Assign your host name or you public IPv4 or IPv6 only
   * as this is the part of EHLO/HELO command to identify the client system
   * to prevent connection rejection.
   * If host name or public IP is not available, ignore this or
   * use generic host "mydomain.net".
   *
   * Assign any text to this option may cause the connection rejection.
   */
  config.login.user_domain = F("mydomain.net");

  /* Set the NTP config time */
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  /* Declare the message class */
  SMTP_Message message;

  /* Enable the chunked data transfer with pipelining for large message if server supported */
  message.enable.chunking = true;

  /* Set the message headers */
  message.sender.name = F("ESP Mail");
  message.sender.email = AUTHOR_EMAIL;

  message.subject = F("Take photo from ESP32-CAM");
  message.addRecipient(F("user1"), RECIPIENT_EMAIL);

  message.html.content = F("<span style=\"color:#ff0000;\">The camera image.</span><br/><br/><img src=\"cid:image-001\" alt=\"esp32 cam image\"  width=\"2048\" height=\"1536\">");

  /** The content transfer encoding e.g.
   * enc_7bit or "7bit" (not encoded)
   * enc_qp or "quoted-printable" (encoded) <- not supported for message from blob and file
   * enc_base64 or "base64" (encoded)
   * enc_binary or "binary" (not encoded)
   * enc_8bit or "8bit" (not encoded)
   * The default value is "7bit"
   */
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /** The HTML text message character set e.g.
   * us-ascii
   * utf-8
   * utf-7
   * The default value is utf-8
   */
  message.html.charSet = F("utf-8");

  // camera_fb_t *fb = esp_camera_fb_get();

  SMTP_Attachment att;

  /** Set the inline image info e.g.
   * file name, MIME type, file path, file storage type,
   * transfer encoding and content encoding
   */
  att.descr.filename = F("camera.jpg");
  att.descr.mime = F("image/jpg");

  att.blob.data = fb->buf;
  att.blob.size = fb->len;

  att.descr.content_id = F("image-001"); // The content id (cid) of camera.jpg image in the src tag

  /* Need to be base64 transfer encoding for inline image */
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;

  /* Add inline image to the message */
  message.addInlineImage(att);

  /* Connect to the server */
  if (!smtp.connect(&config))
  {
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return ESP_FAIL;
  }

  if (smtp.isAuthenticated())
    Serial.println("\nSuccessfully logged in.");
  else
    Serial.println("\nConnected with no Auth.");

  /* Start sending the Email and close the session */
  if (!MailClient.sendMail(&smtp, &message, true))
    ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  // to clear sending result log
  // smtp.sendingResult.clear();

  ESP_MAIL_PRINTF("Free Heap: %d\n", MailClient.getFreeHeap());
#pragma endregion

  Serial.println("Taking picture END");
  esp_camera_fb_return(fb);

  return res;
}
#pragma endregion

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
#pragma region camera
  // 定时拍照发送
  current_millis = millis();
  if (current_millis - last_capture_millis > capture_interval)
  { // Take another picture
    last_capture_millis = millis();
    take_send_photo();
  }
#pragma endregion
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}