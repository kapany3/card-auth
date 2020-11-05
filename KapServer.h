#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "KapObjects.h"

#ifndef KapServer_h
#define KapServer_h

class KapServer
{
  public:
    KapServer(KapObjects*);
    void process();
    void serverBegin(bool isAP);
    void serverEnd();
    bool isStarted();
  private:
    ESP8266WebServer* _server;
    KapObjects* _kapObjects = NULL;
    bool _isStarted = false;
    bool _isAP = false;
    void handleRoot();
    void handleGetConfig();
    void handleSetConfig();
    void handleNotFound();
    bool handleFileRead(String path);
    String getContentType(String filename);
};

#endif
