#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <ESP8266HTTPClient.h>
#include <MFRC522.h>

#include "KapObjects.h"

#ifndef KapCard_h
#define KapCard_h

#define MFRC522_RST_PIN       D3
#define MFRC522_SS_PIN        D4
#define KEY_LENGTH            (32)
#define BASE64_LENGTH         (44)
#define SIGNATURE_LENGTH      (64)
#define SIGNATURE_ENC_LENGTH  (88)


class KapCard
{
  public:
    KapCard(KapObjects*);
    void process();
    void setRFIDKeys();
  private:
    KapObjects* _kapObjects;
    uint8_t* _privateKey;
    uint8_t* _publicKey;
    char* _publicKeyEnc;
    uint8_t* _signature;
    uint8_t readCard[4];
    uint8_t prevCard[4];
    char* _signatureEncoded;
    byte _touchCountr;
    long int _nocard = 0;
    MFRC522::MIFARE_Key _keyA;
    MFRC522::MIFARE_Key _keyB;
    MFRC522::MIFARE_Key _keyD;
    bool _hasRFIDKeys = false;
    unsigned long _touchTime = 0;
    
    WiFiClientSecure* _httpsClient;

    String hexArray(String s, uint8_t* data, size_t len);
    bool updateCounter(bool same);
    void sendSinature(String data);
    bool readData(byte* buf, int sector, MFRC522::MIFARE_Key* key);
    bool changeKeys(MFRC522::MIFARE_Key* oldKeyA, MFRC522::MIFARE_Key* oldKeyB,
                    MFRC522::MIFARE_Key* newKeyA, MFRC522::MIFARE_Key* newKeyB,
                    int sector);
    bool writeKeys();
    bool writeKey(int sector, byte* key);
};

#endif
