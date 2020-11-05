#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef KapConfigStruct_h
#define KapConfigStruct_h

typedef struct {
  char ssidName[32] = "";
  char ssidPass[32] = "";
  char serverAddr[64] = "";
  char serverUrl[64] = "";
  char serverPort[6] = "";
  char deviceName[32] = "";
  char rfidA[7] = "";
  char rfidB[7] = "";
  bool hasRFID = false;
}  KapConfigParams;

#endif
