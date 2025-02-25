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

unsigned int SwitchPin = 2, USBPowerPin = 36;
bool USBPowerState = false;
unsigned int Mode = 0, Temp_mode = 0;

int brightness = 128;
int fadeAmount = 5;

AsyncWebServer server(80);

// LED PWM 控制
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = map(value, 0, valueMax, 0, 4095);
  duty = pow((float)duty / 4095.0, 2.2) * 4095;
  ledcWrite(channel, duty);
}

void IRAM_ATTR handleButtonPress() {
  static unsigned long lastPressTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastPressTime > 200) { // 200ms 防抖
    portENTER_CRITICAL_ISR(&mux); // 进入临界区，防止多核访问冲突
    Temp_mode = Mode;
    Mode = (Mode + 1) % 4;
    portEXIT_CRITICAL_ISR(&mux); // 退出临界区
  }

  lastPressTime = currentTime;
}



void setup() {
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("正在配置接入点...");

  pinMode(SwitchPin, INPUT); // 触摸按键用下拉模式
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, RISING);


  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP地址: ");
  Serial.println(myIP);

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
      if (mode === 1) { 
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
      Serial.printf("设置模式为: %d\n", Mode);
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
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
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
        ledcAnalogWrite(LEDC_CHANNEL_0, 0);
        break;
      case 1:
        ledcAnalogWrite(LEDC_CHANNEL_0, 255);
        break;
      case 2:
          brightness += fadeAmount;
          if (brightness <= 0 || brightness >= 255) fadeAmount = -fadeAmount;
          ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
        break;
      case 3:
        ledcAnalogWrite(LEDC_CHANNEL_0, 0);
        delay(50);
        ledcAnalogWrite(LEDC_CHANNEL_0, 127);
        delay(50);
        break;
    }
  }
