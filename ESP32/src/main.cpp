#include <Arduino.h> 
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_8_BIT   12
#define LEDC_BASE_FREQ     5000
#define LED_PIN            12

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // 定义一个多核锁

const char* ssid     = "ESP32-LED-1";
const char* password = "";

unsigned int SwitchPin = 2, USBPowerPin = 36,BATPin=37;
unsigned int Mode = 0, Temp_mode = 0;
int USBpower=0,BATvotage=0;
int brightness = 128;
int fadeAmount = 2;

AsyncWebServer server(80);

// LED PWM 控制
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = map(value, 0, valueMax, 0, 4095);
  duty = pow((float)duty / 4095.0, 2.2) * 4095;
  ledcWrite(channel, duty);
}

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

  //Serial.begin(115200);
 // Serial.println();
 // Serial.println("正在配置接入点...");

  pinMode(SwitchPin, INPUT); // 触摸按键用下拉模式
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, CHANGE);


  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  //Serial.print("AP IP地址: ");
  //Serial.println(myIP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 LED 控制</title>
    <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }
    h1 { font-size: 24px; }
    p { font-size: 18px; }
    button { font-size: 18px; margin: 5px; padding: 10px; }
    input[type="range"] { width: 100%; }
    </style>
    </head>
    <body>
    <h1>ESP32 LED 控制</h1>
    <p>电池电压: <span id="batteryVoltage">--</span> V</p>
    <p>电池电量: <span id="batteryLevel" style="font-weight: bold;">--</span>%</p>
    <p>当前模式: <span id="currentMode"></span></p>
    <div>
    <button onclick="setMode(0)">关闭</button>
    <button onclick="setMode(1)">开启</button>
    <button onclick="setMode(2)">呼吸</button>
    <button onclick="setMode(3)">闪烁</button>
    </div>
    <p>亮度控制: <input type="range" min="0" max="255" value="128" id="brightnessRange" onchange="setBrightness(this.value)" disabled></p>
    <script>
    function setMode(mode) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/setMode?value=' + mode, true);
      xhr.send();
      if (mode === 1 || mode === 3) { 
        document.getElementById('brightnessRange').disabled = false;
      } else {
        document.getElementById('brightnessRange').disabled = true;
        document.getElementById('brightnessRange').value = 0;
      }
    }
    function setBrightness(value) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/brightness?value=' + value, true);
      xhr.send();
    }
    function updateBatteryVoltage() {
      fetch('/battery')
        .then(response => response.json())
        .then(data => {
          const voltageSpan = document.getElementById('batteryVoltage');
          const levelSpan = document.getElementById('batteryLevel');
          voltageSpan.innerText = data.voltage;
          levelSpan.innerText = data.percentage;

          // 电量颜色逻辑
          if (data.percentage <= 30) {
            levelSpan.style.color = "red";
          } else if (data.percentage <= 60) {
            levelSpan.style.color = "orange";
          } else {
            levelSpan.style.color = "green";
          }
        });
    }
    setInterval(updateBatteryVoltage, 1000);// 每 1 秒更新一次电池电压和电量
    updateBatteryVoltage();

    </script>
    </body>
    </html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (request->hasParam("value")) {
    String modeValue = request->getParam("value")->value();
    Mode = modeValue.toInt();
    //Serial.printf("设置模式为: %d\n", Mode);
    if (Mode == 2) {
      brightness = 0;
    }
  }
  request->send(200, "text/plain", "OK");
});

server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (request->hasParam("value")) {
    String brightnessValue = request->getParam("value")->value();
    brightness = brightnessValue.toInt();
    if (Mode == 1 || Mode == 3) {
      ledcAnalogWrite(LEDC_CHANNEL_0, brightness); // 实时更新亮度
    }
  }
  request->send(200, "text/plain", "OK");
});

server.on("/battery", HTTP_GET, [](AsyncWebServerRequest *request){
  float voltage = ((float)BATvotage / 4095.0) * 3.3 * 2.0;
  int percentage = (int)((voltage - 3.0) / (4.2 - 3.0) * 100.0);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;

  String json = "{\"voltage\":" + String(voltage, 2) +
                ",\"percentage\":" + String(percentage) + "}";
  request->send(200, "application/json", json);
});


server.begin();
pinMode(12, INPUT_PULLUP); // 让 GPIO 12 上拉
delay(100);
pinMode(12, OUTPUT);
ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
}

void loop() {
  
    //Serial.printf("当前模式: %d, 亮度: %d\n", Mode, brightness);
    USBpower=analogRead(USBPowerPin);
    BATvotage=analogRead(BATPin);
    //Serial.printf("电池电压: %d\n", BATvotage/4095*3.3*2);
    //Serial.printf("%d\n",USBpower);
    while (USBpower>=2500)//充电断开输出
    {
      while (1)
      {
      for (size_t i = 0; i < 30; i++)
      {
        brightness=i;
       ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
       delay(20);
      }
      for (size_t i = 30; i<1; i--)
      {
        brightness=i;
       ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
       delay(20);
      }
      }
    }
    switch (Mode) {
      case 0:
        ledcAnalogWrite(LEDC_CHANNEL_0, 0);
        break;
      case 1:
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
        break;
      case 2:
          brightness += fadeAmount;
          if (brightness <= 0 || brightness >= 254) fadeAmount = -fadeAmount;
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
