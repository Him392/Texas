#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 全局参数
#define LEDC_CHANNEL_0     0
#define LEDC_TIMER_8_BIT   12
#define LEDC_BASE_FREQ     8191//只给了14bit
#define LED_PIN            12
#define Debug              1   // 1开启打印日志和超控，0关闭
#define SAMPLE_COUNT       10  // 采样次数

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // 定义一个多核锁

// 引脚定义
unsigned int SwitchPin = 2, USBPowerPin = 36, BATPin = 37;

// 全局变量
bool USBPowerState = false, KeyAvailable = true;
unsigned int Mode = 0, Temp_mode = 0;
unsigned long lastLogTime = 0;
int USBpower = 0;
float BATvotage = 0;
int brightness = 128;
int fadeAmount = 1;

// BLE Variables
BLEServer* pServer = NULL;
BLECharacteristic* pModeCharacteristic = NULL;
BLECharacteristic* pBrightnessCharacteristic = NULL;
BLECharacteristic* pKeyCharacteristic = NULL;
BLECharacteristic* pBatteryCharacteristic = NULL;
BLECharacteristic* pUsbCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MODE_CHAR_UUID         "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BRIGHTNESS_CHAR_UUID   "8c3bcf52-4752-46ac-a4de-a89e830e2f5f"
#define KEY_CHAR_UUID          "f0a3cc1e-eeed-426c-9a4f-ee72f778c1ee"
#define BATTERY_CHAR_UUID      "1de83dc6-3474-45e0-a2ef-cf4c01dcdc8c"
#define USB_CHAR_UUID          "6bdab28d-19cd-488f-a42e-be255dfda983"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class ModeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Mode = value[0];
            if (Mode == 2) brightness = 0; // 重置呼吸灯亮度
            Serial.printf("BLE 设置模式: %d\n", Mode);
        }
    }
};

class BrightnessCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            brightness = value[0];
            Serial.printf("BLE 设置亮度: %d\n", brightness);
        }
    }
};

class KeyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            KeyAvailable = value[0];
            Serial.printf("BLE 实体按键状态: %s\n", KeyAvailable ? "启用" : "禁用");
        }
    }
};

// LED PWM 控制
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  uint32_t duty = map(value, 0, valueMax, 0, 4095);
  duty = pow((float)duty / 4095.0, 2.2) * 4095;
  duty = map(duty, 0, 4095, 1000, 4095);
  ledcWrite(channel, duty);
}

// 按键中断处理函数
volatile bool isLongPress = false;
volatile bool buttonHeld = false;
hw_timer_t *pressTimer = NULL;

#define LONG_PRESS_TIME 250 // 250ms

void IRAM_ATTR handleLongPress() {
  if (KeyAvailable == false) return;
  if (buttonHeld) {
    isLongPress = true;
    if (Mode >= 1 && Mode <= 2) {
      Mode = (Mode % 2) + 1;
    }
  }
}

void IRAM_ATTR handleButtonPress() {
  if (KeyAvailable == false) return;
  static unsigned long pressStartTime = 0;
  unsigned long currentTime = millis();

  if (!buttonHeld) {  
    buttonHeld = true;
    isLongPress = false;
    pressStartTime = currentTime;
    timerAlarmWrite(pressTimer, LONG_PRESS_TIME * 1000, false);
    timerRestart(pressTimer);
    timerAlarmEnable(pressTimer);
  } else {  
    unsigned long pressDuration = currentTime - pressStartTime;
    buttonHeld = false;
    timerAlarmDisable(pressTimer);

    if (!isLongPress) {
      if (Mode != 0) {
        Mode = 0;  
      } else if (Mode == 0) {
        Mode = 1;  
      }
    }
  }
  
  // 更新BLE特征值
  if (pModeCharacteristic) {
      uint8_t m = Mode;
      pModeCharacteristic->setValue(&m, 1);
      pModeCharacteristic->notify();
  }
}

float readBatteryVoltage() {
  long sum = 0;
  for(int i = 0; i < SAMPLE_COUNT; i++){
    sum += analogRead(BATPin);
    delayMicroseconds(200);
  }
  float voltage = (sum / SAMPLE_COUNT) * 3.3f / 4095.0f * 2.0f;
  return voltage;
}

void setup() {
  pressTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(pressTimer, &handleLongPress, true);

  Serial.begin(115200);
  Serial.println();
  Serial.println("初始化...");

  pinMode(SwitchPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, CHANGE);

  // 初始化蓝牙
  BLEDevice::init("Texas-Sword-BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pModeCharacteristic = pService->createCharacteristic(
                      MODE_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pModeCharacteristic->addDescriptor(new BLE2902());
  pModeCharacteristic->setCallbacks(new ModeCallbacks());
  uint8_t initialMode = Mode;
  pModeCharacteristic->setValue(&initialMode, 1);

  pBrightnessCharacteristic = pService->createCharacteristic(
                      BRIGHTNESS_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pBrightnessCharacteristic->setCallbacks(new BrightnessCallbacks());
  uint8_t initialBr = brightness;
  pBrightnessCharacteristic->setValue(&initialBr, 1);

  pKeyCharacteristic = pService->createCharacteristic(
                      KEY_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pKeyCharacteristic->addDescriptor(new BLE2902());
  pKeyCharacteristic->setCallbacks(new KeyCallbacks());
  uint8_t initialKey = KeyAvailable;
  pKeyCharacteristic->setValue(&initialKey, 1);

  pBatteryCharacteristic = pService->createCharacteristic(
                      BATTERY_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pBatteryCharacteristic->addDescriptor(new BLE2902());

  pUsbCharacteristic = pService->createCharacteristic(
                      USB_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pUsbCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  pinMode(LED_PIN, INPUT_PULLUP);
  delay(100);
  pinMode(LED_PIN, OUTPUT);

  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
  
  Serial.println("BLE 已经开启并开始广播.");
}

void loop() {
  if (millis() - lastLogTime >= 500) { 
    lastLogTime = millis();

    USBpower = analogRead(USBPowerPin);
    USBPowerState = (USBpower > 2500);
    BATvotage = readBatteryVoltage();

    if (Debug) {
      Serial.printf(
        "模式:%d | 亮度:%d | 电压:%.2fV | USB:%s\n",
        Mode, brightness, BATvotage,
        USBPowerState ? "插入" : "拔出"
      );
    }
    
    // 如果蓝牙连接，通知状态更新
    if (deviceConnected) {
        uint8_t usb = USBPowerState ? 1 : 0;
        pUsbCharacteristic->setValue(&usb, 1);
        pUsbCharacteristic->notify();
        
        pBatteryCharacteristic->setValue(BATvotage);
        pBatteryCharacteristic->notify();
    }
  }

  // 处理蓝牙重连广告
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // 留出时间给蓝牙栈准备
      pServer->startAdvertising(); // 重新广播
      Serial.println("重连: 开始广播");
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }

  if (USBPowerState && !Debug) {
    Mode=0;
    if (pModeCharacteristic) {
        uint8_t m = Mode;
        pModeCharacteristic->setValue(&m, 1);
        pModeCharacteristic->notify();
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
