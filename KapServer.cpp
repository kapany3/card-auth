#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <Base64.h>

#include "KapServer.h"
#include "KapConfig.h"
#include "KapNetwork.h"
#include "KapObjects.h"
#include "KapCard.h"

KapServer::KapServer(KapObjects* kapObjects) {
  _kapObjects = kapObjects;
  _server = new ESP8266WebServer(80);
  Serial.println("Server created");
}

void KapServer::process() {
  _server->handleClient();
}

bool KapServer::isStarted() {
  return _isStarted;
}

void KapServer::serverBegin(bool isAP) {
  Serial.println("Start server");
  _isAP = isAP;
  _server->on("/", std::bind(&KapServer::handleRoot, this));
  _server->onNotFound(std::bind(&KapServer::handleNotFound, this));
  if (isAP) {
    _server->on("/getConfig", std::bind(&KapServer::handleGetConfig, this));
    _server->on("/setConfig", std::bind(&KapServer::handleSetConfig, this));
  }
  _server->begin();
  _isStarted = true;
}

void KapServer::serverEnd() {
  if (_isStarted) {
    Serial.println("Stop server");
    _server->stop();
    _isStarted = false;
  }
}

void KapServer::handleRoot() {
  String message = "";
  uint8_t argsCount = _server->args();

  if (_isAP) {
    handleFileRead("/config.html");
  } else {
    _server->send(200, "text/plain", "Hello");
  }
}

void KapServer::handleGetConfig() {
  String message = "";
  StaticJsonDocument<512> doc;

  KapConfigParams* conf = _kapObjects->_config->getConfig();

  doc["ssidName"] = conf->ssidName;
  doc["ssidPass"] = conf->ssidPass;
  doc["apPass"] = conf->apPass;
  doc["serverAddr"] = conf->serverAddr;
  doc["serverPort"] = conf->serverPort;
  doc["serverUrl"] = conf->serverUrl;
  doc["deviceName"] = conf->deviceName;

  if (serializeJson(doc, message) == 0) {
    Serial.println(F("Failed to write to string"));
  }

  _server->send(200, "text/plain", message);
}

void KapServer::handleSetConfig() {
  String message = "OK";
  KapConfigParams* conf = _kapObjects->_config->getConfig();

  uint8_t argsCount = _server->args();
  char buf[64];
  for (int i = 0; i < argsCount; i += 1) {
    String argName = _server->argName(i);
    Serial.println("Argument: " + argName);
    _server->arg(i).toCharArray(buf, 64);
    Serial.println("Value: " + String(buf));

    if (argName == "ssid_name") {
      strlcpy(conf->ssidName, buf, sizeof(conf->ssidName));
    } else if (argName == "ssid_pass") {
      strlcpy(conf->ssidPass, buf, sizeof(conf->ssidPass));
    } else if (argName == "ap_pass") {
      strlcpy(conf->apPass, buf, sizeof(conf->apPass));
    } else if (argName == "serverAddr") {
      strlcpy(conf->serverAddr, buf, sizeof(conf->serverAddr));
    } else if (argName == "serverPort") {
      strlcpy(conf->serverPort, buf, sizeof(conf->serverPort));
    } else if (argName == "serverUrl") {
      strlcpy(conf->serverUrl, buf, sizeof(conf->serverUrl));
    } else if (argName == "deviceName") {
      strlcpy(conf->deviceName, buf, sizeof(conf->deviceName));
    } else if (argName == "rfid" && String(buf).length() == 16) {
      Serial.println("Store rfid key");
      uint8_t rfidBuf[13];
      BASE64::decode(buf, rfidBuf);
      strlcpy(conf->rfidA, (char*)&rfidBuf, 7);
      strlcpy(conf->rfidB, (char*)&rfidBuf[6], 7);
      conf->hasRFID = true;
      Serial.println("Keys set in config");
      delay(1000);
      _kapObjects->_card->setRFIDKeys();
    }
  }
  _kapObjects->_config->saveConfig();
  _server->send(200, "text/plain", message);
}

void KapServer::handleNotFound() {
  if (!handleFileRead(_server->uri())) {
    // _server->send(404, "text/plain", "404: Not Found");

    _server->sendHeader("Location", "/", true); //Redirect to our html web page 
    _server->send(302, "text/plane","");
  }
}

String KapServer::getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool KapServer::handleFileRead(String path) {  // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = _server->streamFile(file, contentType);
    file.close();
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;
}
