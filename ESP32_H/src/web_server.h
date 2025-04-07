#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
extern unsigned int Mode;
extern int brightness;

void setupWebServer();
extern void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax);

#endif // WEB_SERVER_H