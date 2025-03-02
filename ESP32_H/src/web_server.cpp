#include "web_server.h"

#define LEDC_CHANNEL_0     0

AsyncWebServer server(80);

void setupWebServer() {
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
      if (Mode == 1 || Mode == 3) {
        ledcAnalogWrite(LEDC_CHANNEL_0, brightness); // 实时更新亮度
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}