#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()

#include "KapNetwork.h"
#include "KapConfig.h"
#include "KapConfigStruct.h"
#include "KapServer.h"
#include "KapCard.h"
#include "KapLed.h"
#include "KapObjects.h"



const int LED_PIN = D0;

timeval cbtime;

KapNetwork::KapNetwork(int port, KapObjects* kapObjects) {
  _kapObjects = kapObjects;
  _kapObjects->_server = new KapServer(kapObjects);

  KapConfigParams* conf = _kapObjects->_config->getConfig();

  if (conf->ssidName[0] == 0) {
    Serial.println("No WiFi connection information available.");
    startAP();
  } else if (!conf->hasRFID) {
    Serial.println("No RFID keys available");
    delay(1000);
    startAP();
  } else {
    Serial.println("ssidName exists");
    Serial.println(conf->ssidName);
    startStation(30);
  }

  if (!MDNS.begin("card-auth")) {
    Serial.println("Error setting up MDNS responder!");
  }  
}

void KapNetwork::toggleMode() {
  Serial.println("Toggle mode");
  if (_apMode == WIFI_STA) {
    startAP();
  } else {
    startStation(60);
  }
}


void KapNetwork::startAP() {
  _kapObjects->_server->serverEnd();
  Serial.println("Starting AP");
  _kapObjects->_led->on();
  _apMode = WIFI_AP;
  WiFi.mode(WIFI_AP);
  delay(10);
  IPAddress local_IP(192,168,10,1);
  IPAddress gateway(192,168,10,1);
  IPAddress subnet(255,255,255,0);
  if (WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP config OK");
  } else {
    Serial.println("AP config FAILED");
  }
  WiFi.softAP("CARDAUTH", "NEVALINK_5_nevalink");
  _kapObjects->_server->serverBegin(true);
  Serial.println("AP PARAMS:\nSSID: CARDAUTH\nPASS: NEVALINK_5_nevalink\nIP: 192.168.10.1");
  if (_dns == NULL) {
    _dns = new DNSServer();
  }
  _dns->start(53, "*", local_IP);

}

char* KapNetwork::getLocalTime()
{
  time_t rawtime;
  struct tm * timeinfo;

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (_timeBuf, 50, "%Y%m%d%H%M%S",timeinfo);
  return _timeBuf;
}

void KapNetwork::startStation(int waitSecs) {
  if (_apMode != WIFI_STA && _kapObjects->_server->isStarted()) {
    _kapObjects->_server->serverEnd();
    if (_dns != NULL) {
      _dns->stop();
    }
  }
  Serial.println("Starting station");
  _apMode = WIFI_STA;
  
  KapConfigParams* conf = _kapObjects->_config->getConfig();

  WiFi.mode(WIFI_STA);
  WiFi.begin(conf->ssidName, conf->ssidPass);

  Serial.println("Trying to connect to ssid " + String(conf->ssidName));

  // Wait for connection
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < waitSecs) {
    delay(1000);
    Serial.print(".");
    counter += 1;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to " + String(conf->ssidName));
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    configTime(3600 * 3, 0, "pool.ntp.org");
    Serial.print("Waiting for time: ");
    while (time(nullptr) < 60) {
      _kapObjects->_led->toggle();
      Serial.print(".");
      delay(250);
    }
    Serial.print(getLocalTime());
    _kapObjects->_led->off();
  } else {
    Serial.println("\nFailed to connect");
    _disconnectTime = millis();
  }
}

bool KapNetwork::isConnected() {
  return _apMode == WIFI_STA && WiFi.status() == WL_CONNECTED;
}

void KapNetwork::checkConnection() {
  if (_apMode == WIFI_AP) {
    _kapObjects->_server->process();
    if (_dns != NULL) {
      _dns->processNextRequest();
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    if (_disconnectTime != 0) {
      Serial.println("WiFi connected");
      _kapObjects->_led->off();
      _disconnectTime = 0;
    }
    // Если в режиме точки доступа или подключен к роутеру
    if (_apMode == WIFI_STA) {
      _kapObjects->_card->process();
    }
  } else if (_apMode == WIFI_STA) {
    // Информировать об отключении
    unsigned long now = millis();
    if (now - _disconnectTime > 1000 || now < _disconnectTime) {
      _disconnectTime = now;
      _kapObjects->_led->toggle();
    }
  }
}
