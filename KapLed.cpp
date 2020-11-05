#include <Arduino.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "KapLed.h"
#include "KapObjects.h"

KapLed::KapLed(int pin) {
  _pin = pin;
  _status = false;
  pinMode(_pin, OUTPUT);
  Serial.println("Led created");
}

void KapLed::on() {
  digitalWrite(_pin, HIGH);
  _status = true;
}

void KapLed::off() {
  digitalWrite(_pin, LOW);
  _status = false;
}

void KapLed::toggle() {
  if (_status) {
    off();
  } else {
    on();
  }
}
