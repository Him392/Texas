#ifndef LED_PWM_H
#define LED_PWM_H

#include <Arduino.h>

// 声明 ledcAnalogWrite 函数
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255);

#endif // LED_PWM_H