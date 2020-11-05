#include <Arduino.h>

#ifndef KapObjects_h
#define KapObjects_h

class KapConfig;
class KapServer;
class KapNetwork;
class KapCard;
class KapLed;

class KapObjects
{
  public:
    KapObjects();
    
    KapConfig* _config = NULL;
    KapServer* _server = NULL;
    KapNetwork* _network = NULL;
    KapCard* _card = NULL;
    KapLed* _led = NULL;
};

#endif
