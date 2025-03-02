#include "button_handler.h"

volatile bool isLongPress = false;
volatile bool buttonHeld = false;
hw_timer_t *pressTimer = NULL;

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

void setupButtonHandler() {
  // 初始化定时器（1 号定时器，80 分频，即 1us 计数）
  pressTimer = timerBegin(1, 80, true);
  timerAttachInterrupt(pressTimer, &handleLongPress, true);

  pinMode(SwitchPin, INPUT); // 触摸按键用下拉模式
  attachInterrupt(digitalPinToInterrupt(SwitchPin), handleButtonPress, CHANGE);
}