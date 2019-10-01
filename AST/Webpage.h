#ifndef _WEBPAGE_H
#define _WEBPAGE_H

#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

class Webpage
{
  public:
    Webpage();
    void checkNetwork();

  private:
    WiFiServer *httpServer;
    bool connectWiFi();

    char readNextClientChar (WiFiClient client, uint32_t *to);
    void scrub(char *s);

    void overrideValue(WiFiClient client);
    void sendNewValues(WiFiClient client);

    void sendMainPage(WiFiClient client);
    void sendPlainHeader(WiFiClient client);
    void sendHTMLHeader(WiFiClient client);
    void sendCompressedHTMLHeader(WiFiClient client);
    void sendEmptyResponse(WiFiClient client);
    void send404Page(WiFiClient client);

    void reboot();
};

// extern Webpage *webpage;

#endif // _WEBPAGE_H
