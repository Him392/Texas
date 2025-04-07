#include <Arduino.h> 
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "web_server.h"
#include "button_handler.h"
#include "led_pwm.h"

#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_8_BIT   12
#define LEDC_BASE_FREQ     5000
#define LED_PIN            12

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // 定义一个多核锁

const char* ssid     = "ESP32-LED-1";
const char* password = "";

unsigned int SwitchPin = 2, USBPowerPin = 36;
bool USBPowerState = false;
unsigned int Mode = 0, Temp_mode = 0;

int brightness = 128;
int fadeAmount = 2;

AsyncWebServer server(80);

// 按键中断处理函数
volatile bool isLongPress = false;  // 记录是否已触发长按
volatile bool buttonHeld = false;   // 按键是否被按住
hw_timer_t *pressTimer = NULL;      // 定时器

#define LONG_PRESS_TIME 250 // 250ms 作为长按阈值

void IRAM_ATTR handleLongPress() {
  if (buttonHeld) {  // 只有按住时才触发
    isLongPress = true;
    if (Mode >= 1 && Mode <= 3) {
      Mode = (Mode % 3) + 1;  // 1 → 2 → 3 → 1 循环
    }
  }
}

void IRAM_ATTR handleButtonPress() {
  static unsigned long pressStartTime = 0;
  unsigned long currentTime = millis();

  if (!buttonHeld) {  
    buttonHeld = true;
    isLongPress = false; // 重置长按状态
    pressStartTime = currentTime;
    timerAlarmWrite(pressTimer, LONG_PRESS_TIME * 1000, false); // 250ms 后触发
    timerRestart(pressTimer);
    timerAlarmEnable(pressTimer);
  } else {  
    unsigned long pressDuration = currentTime - pressStartTime;
    buttonHeld = false;
    timerAlarmDisable(pressTimer); // 释放按键后禁用定时器

    if (!isLongPress) {  // 如果长按已经触发，就不执行短按逻辑
      if (Mode != 0) {
        Mode = 0;  
      } else if (Mode == 0) {
        Mode = 1;  
      }
    }
  }
}

void setup() {
  // 初始化定时器（1 号定时器，80 分频，即 1us 计数）
  pressTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(pressTimer, &handleLongPress, true);

  Serial.begin(115200);
  Serial.println();
  Serial.println("正在配置接入点...");

  pinMode(SwitchPin, INPUT); // 触摸按键用下拉模式
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, CHANGE);


  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP地址: ");
  Serial.println(myIP);

  setupWebServer();

  pinMode(12, INPUT_PULLUP); // 让 GPIO 12 上拉
delay(100);
pinMode(12, OUTPUT);
ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
}

void loop() {
  
  Serial.printf("当前模式: %d, 亮度: %d\n", Mode, brightness);
    switch (Mode) {
      case 0:
        ledcAnalogWrite(LEDC_CHANNEL_0, 0,);
        break;
      case 1:
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
        break;
      case 2:
          brightness += fadeAmount;
          if (brightness <= 0 || brightness >= 255) fadeAmount = -fadeAmount;
          ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
          delay(20);
        break;
      case 3:
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
        delay(100);
        ledcAnalogWrite(LEDC_CHANNEL_0, 0);
        delay(100);
        break;
    }
  }
