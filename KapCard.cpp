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
  
  for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
    _keyD.keyByte[i] = 0xFF;
  }

  _privateKey = new uint8_t[KEY_LENGTH + 2];
  _publicKey = new uint8_t[KEY_LENGTH + 2];
  _publicKeyEnc = new char[BASE64_LENGTH + 1];
  _signature = new uint8_t[SIGNATURE_LENGTH];
  _signatureEncoded = new char[SIGNATURE_ENC_LENGTH + 1];
  memset(_signatureEncoded, 0, SIGNATURE_ENC_LENGTH + 1);
  memset(_publicKeyEnc, 0, BASE64_LENGTH + 1);
  if (conf->hasRFID) {
    setRFIDKeys();
  }

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
  http.begin(*_httpsClient, "https://" + String(conf->serverAddr) + ":" + conf->serverPort + String(conf->serverUrl) + "?data=" + data + "&sig=" + String(_signatureEncoded) + "&pub=" + String(_publicKeyEnc));

  String payload;
  if (http.GET() == HTTP_CODE_OK) {
    Serial.print("Result: ");
    payload = http.getString();  
    Serial.println(payload);
  } else {
    Serial.println("Failure send to server");
  }
  http.end();
}



void KapCard::process() {
  if (!_hasRFIDKeys) {
    return;
  }

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
      && piccType != MFRC522::PICC_TYPE_MIFARE_1K
      && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("This device only works with MIFARE Classic cards."));
    return;
  }
  
  _kapObjects->_led->on();

  if (!readData(_privateKey, 14, &_keyA)) {
    // Не удалось прочитать блоки с секретным ключом
    // Сбрасываю карту, чтоб читать с ключом по умолчанию
    mfrc522.PCD_StopCrypto1();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_Init();
    Serial.println(F("WAIT 2"));
    if (!mfrc522.PICC_IsNewCardPresent()) {
      delay(5);
    }
    while (!mfrc522.PICC_ReadCardSerial()) {
      delay(5);
    }
    Serial.println(F("TRY WRITE NEW KEYS"));
    
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();

    if (!writeKeys()) {
      _kapObjects->_led->off();
      mfrc522.PICC_HaltA(); // Stop reading
      mfrc522.PCD_StopCrypto1();
      mfrc522.PCD_Init();
      return;
    }
  } else {
    // Serial.println(hexArray("Private: ", _privateKey, KEY_LENGTH));
    if (!readData(_publicKey, 15, &_keyA)) {
      _kapObjects->_led->off();
      mfrc522.PICC_HaltA(); // Stop reading
      mfrc522.PCD_StopCrypto1();
      mfrc522.PCD_Init();
      return;
    }
    Serial.println(hexArray("Public: ", _publicKey, KEY_LENGTH));
  }
  

  bool same = true;
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    if (readCard[i] != prevCard[i]) {
      same = false;
      prevCard[i] = readCard[i];
    }
  }

  BASE64::encode(_publicKey, KEY_LENGTH, _publicKeyEnc);

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

  if (!updateCounter(same)) {
    sendSinature(s);
    _kapObjects->_led->off();
    Serial.print("Finished: ");
    Serial.println(_kapObjects->_network->getLocalTime());
    Serial.println("");
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
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

void KapCard::setRFIDKeys() {
  KapConfigParams* conf = _kapObjects->_config->getConfig();
  if (conf->hasRFID) {
    Serial.println("READ RFID KEY FOR CARD A");
    memcpy((char*)_keyA.keyByte, (char*)conf->rfidA, 6);
    // Serial.println(hexArray("keyA: ", (uint8_t*)conf->rfidA, 6));
    Serial.println("READ RFID KEY FOR CARD B");
    memcpy((char*)_keyB.keyByte, (char*)conf->rfidB, 6);
    // Serial.println(hexArray("keyB: ", (uint8_t*)conf->rfidB, 6));
    _hasRFIDKeys = true;
    // Serial.println(hexArray("keyD: ", _keyD.keyByte, 6));
  }
}

bool KapCard::readData(byte* buf, int sector, MFRC522::MIFARE_Key* key) {
  MFRC522::StatusCode status;
  int block;
  byte size = 18;
  for (int blockInc = 0; blockInc < 2; blockInc += 1) {
    block = 4 * sector + blockInc;
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("PCD_Authenticate() failed for block "));
      Serial.print(block);
      Serial.print(F(": "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }
    
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(block, &buf[16 * blockInc], &size);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed for block "));
      Serial.print(block);
      Serial.print(F(": "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }
  }
  return true;
}

bool KapCard::changeKeys(MFRC522::MIFARE_Key* oldKeyA, MFRC522::MIFARE_Key* oldKeyB,
                    MFRC522::MIFARE_Key* newKeyA, MFRC522::MIFARE_Key* newKeyB,
                    int sector) {
  MFRC522::StatusCode status;
  byte trailerBlock = sector * 4 + 3;
  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key A
  // Serial.print(F("Authenticating using old key A..."));
  Serial.println(hexArray("Authenticating using old key A...", oldKeyA->keyByte, 6));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, oldKeyA, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed while change keys: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Show the whole sector as it currently is
  Serial.println(F("Current data in sector:"));
  mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), oldKeyA, sector);
  Serial.println();

  // Read data from the block
  Serial.print(F("Reading data from block ")); Serial.println(trailerBlock);
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(trailerBlock, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed while change key: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  /*
  Serial.print(F("Data in block ")); Serial.print(trailerBlock); Serial.println(F(":"));
  dump_byte_array(buffer, 16); Serial.println();
  Serial.println();
  */

  // Authenticate using key B
  Serial.println(F("Authenticating again using old key B..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, oldKeyB, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  if (newKeyA != nullptr || newKeyB != nullptr) {
    for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
      if (newKeyA != nullptr) {
        buffer[i] = newKeyA->keyByte[i];
      }
      if (newKeyB != nullptr) {
        buffer[i+10] = newKeyB->keyByte[i];
      }
    }
  }

  // Write data to the block
  Serial.print(F("Writing data into block ")); Serial.println(trailerBlock);
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(trailerBlock, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Read data from the block (again, should now be what we have written)
  Serial.print(F("Reading data from block ")); Serial.println(trailerBlock);
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(trailerBlock, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  /*
  Serial.print(F("Data in block ")); Serial.print(trailerBlock); Serial.println(F(":"));
  dump_byte_array(buffer, 16); Serial.println();
  */
  return true;
}

bool KapCard::writeKeys() {
  MFRC522::StatusCode status;
  
  if (!changeKeys(&_keyD, &_keyD, &_keyA, &_keyB, 14)) {
    Serial.println(F("ERROR CHANGE RFID KEY 14"));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    mfrc522.PCD_Init();
    return false;
  }
  if (!changeKeys(&_keyD, &_keyD, &_keyA, &_keyB, 15)) {
    Serial.println(F("ERROR CHANGE RFID KEY 15"));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    mfrc522.PCD_Init();
    _kapObjects->_led->off();
    return false;
  }

  uint8_t _priv[32];
  uint8_t _publ[32];
  Ed25519::generatePrivateKey(_privateKey);
  Ed25519::derivePublicKey(_publicKey, _privateKey);
  if (!writeKey(14, _privateKey)) {
    return false;
  }
  if (!writeKey(15, _publicKey)) {
    return false;
  }

  Serial.println("KEYS WAS SUCSESSFULLY WROTE");
  return true;
}

bool KapCard::writeKey(int sector, byte* key) {
  MFRC522::StatusCode status;

  for (int blockInc = 0; blockInc < 2; blockInc += 1) {
    // Authenticate using key A
    byte block = sector * 4 + blockInc;
    
    Serial.println(F("Authenticating using key A..."));
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &_keyA, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("PCD_Authenticate() failed for write key: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }
  
    Serial.print(F("Writing data into block ")); Serial.print(block);
    Serial.println(F(" ..."));
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(block, &key[16 * blockInc], 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }
    Serial.println();
  }
}
