#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_12_BIT  12
#define LEDC_BASE_FREQ     5000
#define LED_PIN            12
#define SWITCH_PIN         2
#define USB_POWER_PIN      36

const char* ssid     = "ESP32-LED-1";
const char* password = "";

bool USBPowerState = false;
unsigned int Mode = 0, Temp_mode = 0;

int brightness = 0;
int fadeAmount = 1;
unsigned long previousMillis = 0;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;

AsyncWebServer server(80);

// LED PWM 控制
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = map(value, 0, valueMax, 0, 4095);
  duty = pow((float)duty / 4095.0, 2.2) * 4095;
  ledcWrite(channel, duty);
}

// 处理按键中断（非阻塞）
void IRAM_ATTR handleButtonPress() {
  buttonPressTime = millis();
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("正在配置接入点...");
  
  // LED PWM 设置
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
  
  // 按键中断
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(SWITCH_PIN, handleButtonPress, FALLING);

  // WiFi AP模式
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP地址: ");
  Serial.println(myIP);

  // 网页控制
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    int VCC = analogReadMilliVolts(USB_POWER_PIN);
    USBPowerState = VCC > 1000;
    String modeText;
    switch (Mode) {
      case 0: modeText = "关闭"; break;
      case 1: modeText = "开启"; break;
      case 2: modeText = "呼吸"; break;
      case 3: modeText = "手动"; break;
    }

    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 LED 控制</title>
    <script>
    function setMode(mode) { var xhr = new XMLHttpRequest(); xhr.open('GET', '/setMode?value=' + mode, true); xhr.send(); }
    function setBrightness(value) { var xhr = new XMLHttpRequest(); xhr.open('GET', '/brightness?value=' + value, true); xhr.send(); }
    </script>
    </head>
    <body>
    <h1>ESP32 LED 控制</h1>
    <p>USB 电源状态: <span id="usbPowerState">)rawliteral" + String(USBPowerState) + R"rawliteral(</span></p>
    <p>VCC: <span id="vcc">)rawliteral" + String(VCC * 2) + R"rawliteral( mV</span></p>
    <p>当前模式: <span id="currentMode">)rawliteral" + modeText + R"rawliteral(</span></p>
    <button onclick="setMode(0)">关闭</button>
    <button onclick="setMode(1)">开启</button>
    <button onclick="setMode(2)">呼吸</button>
    <button onclick="setMode(3)">手动</button>
    <p>亮度 (手动模式): <input type="range" min="0" max="255" id="brightnessRange" onchange="setBrightness(this.value)"></p>
    </body>
    </html>
    )rawliteral";

    request->send(200, "text/html", html);
  });

  server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
      String modeValue = request->getParam("value")->value();
      Mode = modeValue.toInt();
      Serial.printf("模式切换为: %d\n", Mode);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
      String brightnessValue = request->getParam("value")->value();
      brightness = brightnessValue.toInt();
      Serial.printf("亮度设置为: %d\n", brightness);
      if (Mode == 3) ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  // 读取 USB 供电状态
  int VCC = analogReadMilliVolts(USB_POWER_PIN);
  USBPowerState = VCC > 1000;// 1V

  // USB 充电时，LED 呼吸效果
  if (USBPowerState) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 30) {
      previousMillis = currentMillis;
      brightness += fadeAmount;
      if (brightness <= 0 || brightness >= 30) {
        fadeAmount = -fadeAmount;
      }
      ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
    }
    return;  // 直接返回，避免执行后续模式
  }

  // 处理按键（非阻塞）
  if (buttonPressed) {
    buttonPressed = false;
    unsigned long pressDuration = millis() - buttonPressTime;
    
    if (pressDuration > 800) { // 长按切换模式
      Mode = (Mode + 1) % 4;
    } else { // 短按开关
      if (Mode != 0) {
        Temp_mode = Mode;
        Mode = 0;
      } else {
        Mode = Temp_mode;
      }
    }
  }

  // 按模式控制 LED
  switch (Mode) {
    case 0:
      ledcAnalogWrite(LEDC_CHANNEL_0, 0);
      break;
    case 1:
      ledcAnalogWrite(LEDC_CHANNEL_0, 30);
      break;
    case 2:
      if (millis() - previousMillis >= 30) {
        previousMillis = millis();
        brightness += fadeAmount;
        if (brightness <= 0 || brightness >= 30) fadeAmount = -fadeAmount;
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      }
      break;
    case 3:
      // 手动模式，亮度由网页控制
      break;
  }
}
