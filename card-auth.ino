#include "KapConfig.h"
#include "KapNetwork.h"
#include "KapCard.h"
#include "KapLed.h"

KapObjects* kapObjects;
/*
#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#define DEBUG_WEBSOCKETS(...) os_printf( __VA_ARGS__ )
#else
#define DEBUG_MSG(...)
#endif
*/

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n\n\nHello, All");
  
  kapObjects = new KapObjects();
  kapObjects->_led = new KapLed(D0);
  kapObjects->_led->on();
  kapObjects->_config = new KapConfig();
  kapObjects->_card = new KapCard(kapObjects);
  kapObjects->_network = new KapNetwork(80, kapObjects);

  Serial.println("\nInit finished");
}

void loop() {
  kapObjects->_network->checkConnection();  
}
