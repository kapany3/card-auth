#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef KapConfigStruct_h
#define KapConfigStruct_h

typedef struct {
  char ssidName[32];
  char ssidPass[32];
  char serverAddr[64];
  char serverUrl[64];
  char serverPort[6];
  char deviceName[32];
  char privKey[64];
  char publKey[64];
}  KapConfigParams;

#endif
