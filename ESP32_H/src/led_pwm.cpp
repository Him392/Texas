#include <Arduino.h>

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
    uint32_t duty = map(value, 0, valueMax, 0, 4095);
    duty = pow((float)duty / 4095.0, 2.2) * 4095;
    ledcWrite(channel, duty);
  }