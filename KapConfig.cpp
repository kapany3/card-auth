#include <Arduino.h>
#include <ArduinoJson.h>
#include <Base64.h>

#include "FS.h"
#include "KapConfigStruct.h"
#include "KapConfig.h"
#include "KapObjects.h"

KapConfig::KapConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  loadConfig();
  Serial.println("Config created");
  delay(1000);  
}

void KapConfig::loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config.json");
  }
  Serial.println("Loading config");
  String content = configFile.readString();
  configFile.close();

  // Serial.println(content);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, content);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    _configParams.hasRFID = false;
    return;
  }

  Serial.println("Config parsed");
  
  strlcpy(_configParams.ssidName, doc["ssidName"] | "", sizeof(_configParams.ssidName));
  Serial.println("SSID NAME: " + String(_configParams.ssidName));

  strlcpy(_configParams.ssidPass, doc["ssidPass"] | "", sizeof(_configParams.ssidPass));
  Serial.println("SSID PASS: " + String(_configParams.ssidPass));
  
  strlcpy(_configParams.serverAddr, doc["serverAddr"] | "", sizeof(_configParams.serverAddr));
  Serial.println("Server Addr: " + String(_configParams.serverAddr));
  
  strlcpy(_configParams.serverPort, doc["serverPort"] | "", sizeof(_configParams.serverPort));
  Serial.println("Server Port: " + String(_configParams.serverPort));
  
  strlcpy(_configParams.deviceName, doc["deviceName"] | "", sizeof(_configParams.deviceName));
  Serial.println("Device name: " + String(_configParams.deviceName));
  
  strlcpy(_configParams.serverUrl, doc["serverUrl"] | "", sizeof(_configParams.serverUrl));
  Serial.println("Server Url: " + String(_configParams.serverUrl));
  
  Serial.println("CHECK RFID KEYS");
  if (doc["rfidA"] != "" && doc["rfidB"] != "") {
    Serial.println("DECODE RFID KEYS");
    _configParams.hasRFID = true;
    BASE64::decode(doc["rfidA"], (uint8_t *)&_configParams.rfidA);
    delay(1000);
    Serial.println("NEXT");
    BASE64::decode(doc["rfidB"], (uint8_t *)&_configParams.rfidB);
    delay(1000);
  } else {
    _configParams.hasRFID = false;
    Serial.println("No RFID keys available");
  }
}

void KapConfig::setSsid(const char* ssid, const char* pass) {
  strlcpy(_configParams.ssidName, ssid, sizeof(_configParams.ssidName));
  strlcpy(_configParams.ssidPass, pass, sizeof(_configParams.ssidPass));
}

bool KapConfig::saveConfig() {
  Serial.println("Save config");
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config.json for writing");
    return false;
  }

  StaticJsonDocument<512> doc;

  doc["ssidName"] = _configParams.ssidName;
  doc["ssidPass"] = _configParams.ssidPass;
  doc["serverAddr"] = _configParams.serverAddr;
  doc["serverPort"] = _configParams.serverPort;
  doc["serverUrl"] = _configParams.serverUrl;
  doc["deviceName"] = _configParams.deviceName;

  if (_configParams.hasRFID) {
    char bufA[9];
    memset(bufA, 0, 9);
    BASE64::encode((uint8_t*)&_configParams.rfidA, 6, bufA);
    /*
    Serial.print("keyA: ");
    Serial.println(bufA);
    */
    doc["rfidA"] = bufA;

  
    char bufB[9];
    memset(bufB, 0, 9);
    BASE64::encode((uint8_t*)&_configParams.rfidB, 6, bufB);
    /*
    Serial.print("keyB: ");
    Serial.println(bufB);
    */
    doc["rfidB"] = bufB;
  
  } else {
    doc["rfidA"] = "";
    doc["rfidB"] = "";
  }
  
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  } else {
    Serial.println("Config saved");
  }
  
  configFile.close();
  return true;
}

KapConfigParams* KapConfig::getConfig() {
  return &_configParams;
}
