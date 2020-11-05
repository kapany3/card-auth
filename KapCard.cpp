#include <Base64.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Crypto.h>
#include <Ed25519.h>
#include <RNG.h>
#include <utility/ProgMemUtil.h>
#include <string.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <ESP8266HTTPClient.h>

#include "KapCard.h"
#include "KapObjects.h"
#include "KapConfig.h"
#include "KapNetwork.h"
#include "KapLed.h"
#include "KapConfigStruct.h"


MFRC522 mfrc522(MFRC522_SS_PIN, MFRC522_RST_PIN);

KapCard::KapCard(KapObjects* kapObjects) {
  _kapObjects = kapObjects;
  _touchCountr = 0;

  SPI.begin();
  mfrc522.PCD_Init();
  delay(4); // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  KapConfigParams* conf = _kapObjects->_config->getConfig();
  
  _privateKey = new uint8_t[KEY_LENGTH];
  _publicKey = new uint8_t[KEY_LENGTH];

  if (conf->privKey[0] == 0) {
    Serial.println("Private key is empty");
    regenerate();
    _kapObjects->_config->saveConfig();
  } else {
    //  Декодировать приватный ключ
    // Serial.println("Decode private key");
    // Serial.println(conf->privKey);
    BASE64::decode(conf->privKey, _privateKey);
  
    Serial.println("Decode public key");
    Serial.println(conf->publKey);
    BASE64::decode(conf->publKey, _publicKey);
  }

  _signature = new uint8_t[SIGNATURE_LENGTH];
  _signatureEncoded = new char[SIGNATURE_ENC_LENGTH + 1];
  memset(_signatureEncoded, 0, SIGNATURE_ENC_LENGTH + 1);

  _httpsClient = new WiFiClientSecure();
  _httpsClient->setInsecure();
  
  Serial.println("Card created");
}

bool KapCard::updateCounter(bool same) {
  unsigned long now = millis();
  if (same && (now - _touchTime < 10000)) {
    _touchCountr += 1;

    Serial.print("Touch counter: ");
    Serial.println(_touchCountr);

    if (_touchCountr == 5) {
      _kapObjects->_network->startAP();
      return true;
    }
  } else {
    _touchCountr = 1;
  }
  _touchTime = now;
  return false;
}

void KapCard::sendSinature(String data) {
  KapConfigParams* conf = _kapObjects->_config->getConfig();
  
  // Serial.println("Connecting");
  _httpsClient->connect(conf->serverAddr, String(conf->serverPort).toInt());

  // Serial.println("Begin request");
  HTTPClient http;
  http.begin(*_httpsClient, "https://" + String(conf->serverAddr) + ":" + conf->serverPort + String(conf->serverUrl) + "?data=" + data + "&sig=" + String(_signatureEncoded));

  String payload;
  if (http.GET() == HTTP_CODE_OK) {
    Serial.println("Result:");
    payload = http.getString();  
    Serial.println(payload);
  } else {
    Serial.println("Failure send to server");
  }
  http.end();
}

void KapCard::process() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  _kapObjects->_led->on();

  bool same = true;
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    if (readCard[i] != prevCard[i]) {
      same = false;
      prevCard[i] = readCard[i];
    }
  }

  String s = hexArray(String(_kapObjects->_network->getLocalTime()) + ":" + _kapObjects->_config->getConfig()->deviceName + ":", readCard, 4);
  int len = s.length();
  char charBufVar[35];
  s.toCharArray(charBufVar, len + 1);
    
  Serial.println("");
  Serial.print("DATA: ");
  Serial.println(charBufVar);
  // Serial.print("LENGTH: ");
  // Serial.println(len);

  Ed25519::sign(_signature, _privateKey, _publicKey, charBufVar, len);
  BASE64::encode(_signature, SIGNATURE_LENGTH, _signatureEncoded);
  Serial.print("SIGNATURE: ");
  Serial.println(_signatureEncoded);

/*
  bool eq = Ed25519::verify(_signature, _publicKey, charBufVar, len);
  if (eq) {
    Serial.println("Verified");
  } else {
    Serial.println("Failed");
  }
*/
  mfrc522.PICC_HaltA(); // Stop reading

  if (!updateCounter(same)) {
    sendSinature(s);
    _kapObjects->_led->off();
    Serial.print("Finished ");
    Serial.println(_kapObjects->_network->getLocalTime());
    Serial.println("");
  }
}

String KapCard::hexArray(String s, uint8_t* data, size_t len) {
  String item;
  for (int i = 0; i < len; i += 1) {
    item = String(data[i], HEX);
    if (item.length() == 1) {
      item = "0" + item;
    }
    s += item;
  }
  return s;
}

void KapCard::regenerate() {
  // Создать новый ключ
  Ed25519::generatePrivateKey(_privateKey);
  Ed25519::derivePublicKey(_publicKey, _privateKey);

  // Конвертировать в base64
  char* publicEncoded = new char[BASE64_LENGTH + 1];
  memset(publicEncoded, 0, BASE64_LENGTH + 1);
  BASE64::encode(_publicKey, KEY_LENGTH, publicEncoded);
  Serial.print("New public: ");
  Serial.println(publicEncoded);

  char* privateEncoded = new char[BASE64_LENGTH + 1];
  memset(privateEncoded, 0, BASE64_LENGTH + 1);
  BASE64::encode(_privateKey, KEY_LENGTH, privateEncoded);
  Serial.print("New private: ");
  Serial.println(privateEncoded);

  _kapObjects->_config->setKeys(privateEncoded, publicEncoded);

  delete publicEncoded;
  delete privateEncoded;
}
