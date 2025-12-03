#include <Arduino.h> 
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
//全局参数
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_8_BIT   12
#define LEDC_BASE_FREQ     8191//只给了14bit
#define LED_PIN            12
#define DebugON            1  // 1开启打印日志和超控，0关闭
#define SAMPLE_COUNT       10  // 采样次数

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // 定义一个多核锁

const char* ssid     = "ESP32-LED-1";
const char* password = "";
// 引脚定义
unsigned int SwitchPin = 2, USBPowerPin = 36,BATPin=37;
// 全局变量
bool USBPowerState = false,KeyAvailable=true;
unsigned int Mode = 0, Temp_mode = 0;
unsigned long lastLogTime = 0;
int USBpower=0;
float BATvotage=0;
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
  if (KeyAvailable==false) return;
  
  if (buttonHeld) {  // 只有按住时才触发
    isLongPress = true;
    if (Mode >= 1 && Mode <= 2) {
      Mode = (Mode % 2) + 1;  // 1 → 2  → 1 循环（闪烁为WEB ONLY）
    }
  }
}

void IRAM_ATTR handleButtonPress() {
  if (KeyAvailable==false) return;
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
// ------- 电池电压采样函数（带滤波） -------
float readBatteryVoltage() {
  long sum = 0;
  for(int i = 0; i < SAMPLE_COUNT; i++){
    sum += analogRead(BATPin);
    delayMicroseconds(200); // 小延时让 ADC 更稳定
  }

  float voltage = (sum / SAMPLE_COUNT) * 3.3f / 4095.0f * 2.0f;
  return voltage;
}

void setup() {
  pressTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(pressTimer, &handleLongPress, true);

  Serial.begin(115200);
  Serial.println();
  Serial.println("正在配置热点...");

  pinMode(SwitchPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, CHANGE);

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  esp_wifi_set_max_tx_power(WIFI_POWER_11dBm);
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
body {
  font-family: Arial, sans-serif;
  margin: 0; padding: 20px;
  background: #f2f2f2;
}

h1 { font-size: 26px; margin-bottom: 15px; }

button {
  font-size: 18px; padding: 10px 14px;
  margin: 6px; border: none;
  border-radius: 6px; cursor: pointer;
  background: #2196F3; color: white;
}
button:active { background: #0b7dda; }

input[type="range"] {
  width: 100%;
  margin-top: 5px;
}

.status-box {
  background: white;
  padding: 12px;
  border-radius: 10px;
  box-shadow: 0 2px 5px rgba(0,0,0,0.15);
  margin-bottom: 20px;
  line-height: 1.6;
  font-size: 18px;
}
</style>
</head>
<body>

<h1>ESP32 LED 控制</h1>

<div class="status-box">
<p>模式: <strong><span id="currentMode">--</span></strong></p>
<p>电池电压: <strong><span id="batteryVoltage">--</span> V</strong></p>
<p>USB状态: <strong><span id="usbState">--</span></strong></p>
<p>实体按键: <strong><span id="keyStatus">启用</span></strong></p>
</div>

<button onclick="setMode(0)">关闭</button>
<button onclick="setMode(1)">开启</button>
<button onclick="setMode(2)">呼吸</button>
<button onclick="setMode(3)">闪烁</button>

<p>亮度:</p>
<input type="range" min="0" max="255" value="128" id="brightnessRange"
       onchange="setBrightness(this.value)" disabled>

<hr>

<button onclick="toggleKey()" id="keyBtn">禁用实体按键</button>

<script>
function setMode(mode) {
  fetch('/setMode?value=' + mode);
  document.getElementById("currentMode").innerText = mode;

  document.getElementById("brightnessRange").disabled = !(mode === 1 || mode === 3);
}

function setBrightness(value) {
  fetch('/brightness?value=' + value);
}

function toggleKey() {
  fetch('/toggleKey')
    .then(response => response.text())
    .then(state => {
      if (state === "ENABLED") {
        document.getElementById("keyStatus").innerText = "启用";
        document.getElementById("keyBtn").innerText = "禁用实体按键";
      } else {
        document.getElementById("keyStatus").innerText = "禁用";
        document.getElementById("keyBtn").innerText = "启用实体按键";
      }
    });
}

// ---- 自动刷新状态，每秒从 /status 拉取 ----
setInterval(() => {
  fetch('/status')
    .then(response => response.json())
    .then(data => {
      document.getElementById("currentMode").innerText = data.mode;
      document.getElementById("batteryVoltage").innerText = data.battery.toFixed(2);
      document.getElementById("usbState").innerText = data.usb ? "插入" : "拔出";
    });
}, 1000);

</script>

</body>
</html>
)rawliteral";

    request->send(200, "text/html", html);
});

  // ---- 修改 LED 模式 ----
  server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
      Mode = request->getParam("value")->value().toInt();
      Serial.printf("模式设置: %d\n", Mode);
      if (Mode == 2) brightness = 0;
    }
    request->send(200, "text/plain", "OK");
  });

  // ---- 修改亮度 ----
  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
      brightness = request->getParam("value")->value().toInt();
      if (Mode == 1 || Mode == 3) {
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      }
    }
    request->send(200, "text/plain", "OK");
  });
  // ---- 按键启用 ----
  server.on("/toggleKey", HTTP_GET, [](AsyncWebServerRequest *request) {
    KeyAvailable = !KeyAvailable;  
    Serial.printf("实体按键状态: %s\n", KeyAvailable ? "启用" : "禁用");
    request->send(200, "text/plain", KeyAvailable ? "ENABLED" : "DISABLED");
  });
  // ---- 状态接口(JSON) ----
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"mode\":" + String(Mode) + ",";
    json += "\"battery\":" + String(BATvotage, 2) + ",";
    json += "\"usb\":" + String(USBPowerState);
    json += "}";

    request->send(200, "application/json", json);
  });

  server.begin();

  pinMode(12, INPUT_PULLUP);
  delay(100);
  pinMode(12, OUTPUT);

  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
}


void loop() {
if (millis() - lastLogTime >= 1000) {  // 每秒执行一次
  lastLogTime = millis();  // 先更新时间，避免累积误差

  // USB判定，直接布尔表达
  USBpower = analogRead(USBPowerPin);
  USBPowerState = (USBpower > 2500);

  // 调用滤波函数获取平滑电压
  BATvotage = readBatteryVoltage();

  // 串口日志输出（可关闭）
  if (DebugON) {
    Serial.printf(
      "模式:%d | 亮度:%d | 电压:%.2fV | USB:%s\n",
      Mode, brightness, BATvotage,
      USBPowerState ? "插入" : "拔出"
    );
  }
}

  if (USBPowerState&&!DebugON)
  {
    Mode=0;
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
      delay(10);
      break;
    case 3:
      ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      delay(50);
      ledcAnalogWrite(LEDC_CHANNEL_0, 0);
      delay(50);
      break;
    }
  }
