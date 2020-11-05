#include <Arduino.h>
#include <ArduinoJson.h>
#include "KapConfigStruct.h"
#include "KapObjects.h"

#ifndef KapLed_h
#define KapLed_h

class KapLed {
  public:
    KapLed(int pin);
    void on();
    void off();
    void toggle();
  private:
    int _pin;
    bool _status;
};


#endif
