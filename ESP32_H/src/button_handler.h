#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>

extern volatile bool isLongPress;
extern volatile bool buttonHeld;
extern hw_timer_t *pressTimer;
extern unsigned int Mode;
extern unsigned int SwitchPin;

void setupButtonHandler();
void IRAM_ATTR handleLongPress();
void IRAM_ATTR handleButtonPress();

#endif // BUTTON_HANDLER_H