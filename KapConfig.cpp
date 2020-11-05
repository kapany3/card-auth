#include <Arduino.h>
#include <ArduinoJson.h>
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
  
  strlcpy(_configParams.privKey, doc["private"] | "", sizeof(_configParams.privKey));
  // Serial.println("Private key: " + String(_configParams.privKey));
  
  strlcpy(_configParams.publKey, doc["public"] | "", sizeof(_configParams.publKey));
  Serial.println("Public key: " + String(_configParams.publKey));
}

void KapConfig::setSsid(const char* ssid, const char* pass) {
  strlcpy(_configParams.ssidName, ssid, sizeof(_configParams.ssidName));
  strlcpy(_configParams.ssidPass, pass, sizeof(_configParams.ssidPass));
}
/*
void KapConfig::setServer(const char* server) {
  strlcpy(_configParams.server, server, sizeof(_configParams.server));
}
*/
void KapConfig::setKeys(const char* privateKey, const char* publicKey) {
  strlcpy(_configParams.privKey, privateKey, sizeof(_configParams.privKey));
  strlcpy(_configParams.publKey, publicKey, sizeof(_configParams.publKey));
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
  doc["private"] = _configParams.privKey;
  doc["public"] = _configParams.publKey;

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
