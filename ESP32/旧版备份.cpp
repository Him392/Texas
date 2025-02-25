#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_12_BIT  12
#define LEDC_BASE_FREQ     5000
#define LED_PIN            12

const char* ssid     = "ESP32-LED-1";
const char* password = "";

unsigned int SwitchPin = 2, USBPowerPin = 36;
bool USBPowerState = false;
unsigned int Mode = 0,Temp_mode = 0;

int brightness = 0;
int fadeAmount = 5;

AsyncWebServer server(80);

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = map(value, 0, valueMax, 0, 4095);
  duty = pow((float)duty / 4095.0, 2.2) * 4095;
  ledcWrite(channel, duty);
}


void IRAM_ATTR handleButtonPress() {
  Temp_mode = Mode;
  delay(100);
  if (SwitchPin==HIGH)
  {
    //长按
    Mode = (Mode + 1) % 4;
  }
  else
  {
    //短按
    if (Mode != 0){
      Mode = 0;
    }
    else
    {
      Mode = Temp_mode;
    }
  }
  
}
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("正在配置接入点...");
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
  attachInterrupt(SwitchPin, handleButtonPress, RISING);

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP地址: ");
  Serial.println(myIP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){//网页
    int VCC = analogReadMilliVolts(USBPowerPin);
    USBPowerState = VCC > 1000;
    String modeText;
    switch (Mode) {
      case 0:
      modeText = "关闭";
      break;
      case 1:
      modeText = "开启";
      break;
      case 2:
      modeText = "呼吸";
      break;
      case 3:
      modeText = "手动";
      break;
    }
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
    <p>USB 电源状态: <span id="usbPowerState">)rawliteral" + String(USBPowerState) + R"rawliteral(</span></p>
    <p>VCC: <span id="vcc">)rawliteral" + String(VCC * 2) + R"rawliteral( mV</span></p>
    <p>当前模式: <span id="currentMode">)rawliteral" + modeText + R"rawliteral(</span></p>
    <div>
    <button onclick="setMode(0)">关闭</button>
    <button onclick="setMode(1)">开启</button>
    <button onclick="setMode(2)">呼吸</button>
    <button onclick="setMode(3)">手动</button>
    </div>
    <p>亮度控制 (仅手动模式有效): <input type="range" min="0" max="255" value=")rawliteral" + String(brightness) + R"rawliteral(" id="brightnessRange" onchange="setBrightness(this.value)" )rawliteral" + (Mode != 3 ? "disabled" : "") + R"rawliteral(>
    </p>
    <script>
    function setMode(mode) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/setMode?value=' + mode, true);
      xhr.send();
    }
    function setBrightness(value) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/brightness?value=' + value, true);
      xhr.send();
    }
    function getModeText(mode) {
      switch (mode) {
        case 0:
          return "关闭";
        case 1:
          return "开启";
        case 2:
          return "呼吸";
        case 3:
          return "手动";
        default:
          return "未知";
      }
    }
    function updateStatus() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/status', true);
      xhr.onload = function() {
        if (xhr.status === 200) {
          var data = JSON.parse(xhr.responseText);
          document.getElementById('usbPowerState').innerText = data.usbPowerState;
          document.getElementById('vcc').innerText = data.vcc + ' mV';
          document.getElementById('currentMode').innerText = getModeText(data.mode);
          var brightnessRange = document.getElementById('brightnessRange');
          if (data.mode == 3) {
            brightnessRange.disabled = false;
          } else {
            brightnessRange.disabled = true;
            brightnessRange.value = 0; // 在非手动模式下将滑条值清零
          }
        }
      };
      xhr.send();
    }
    setInterval(updateStatus, 1000);
    </script>
    </body>
    </html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      String brightnessValue = request->getParam("value")->value();
      brightness = brightnessValue.toInt();
      Serial.printf("设置亮度为: %d\n", brightness);
      if (Mode == 3) {
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      String modeValue = request->getParam("value")->value();
      Mode = modeValue.toInt();
      Serial.printf("设置模式为: %d\n", Mode);
      if (Mode == 3 && request->hasParam("brightness")) {
        String brightnessValue = request->getParam("brightness")->value();
        brightness = brightnessValue.toInt();
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    int VCC = analogReadMilliVolts(USBPowerPin);
    USBPowerState = VCC > 1000;
    String json = "{\"usbPowerState\":" + String(USBPowerState) + ",\"vcc\":" + String(VCC * 2) + ",\"mode\":" + String(Mode) + "}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {

  while (USBPowerState == true) {
    brightness += fadeAmount;
    if (brightness <= 0 || brightness >= 30) {
      fadeAmount = -fadeAmount;
    }
    ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
    delay(30);
  }

  switch (Mode) {
    case 0:
      ledcAnalogWrite(LEDC_CHANNEL_0, 0);
      break;
    case 1:
      ledcAnalogWrite(LEDC_CHANNEL_0, 255);
      break;
    case 2:
      brightness += fadeAmount;
      if (brightness <= 0 || brightness >= 255) {
        fadeAmount = -fadeAmount;
      }
      ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
      delay(30);
      break;
    case 3:
      ledcAnalogWrite(LEDC_CHANNEL_0, 0);
      delay(50);
      ledcAnalogWrite(LEDC_CHANNEL_0, 255);
      delay(50);
      break;
    }
  }
//单独测试IO2状态？当前版本触摸按键处理貌似阻塞了逻辑/fixed
