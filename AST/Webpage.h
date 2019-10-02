#ifndef _WEBPAGE_H
#define _WEBPAGE_H

#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

#include "GPS.h"
#include "IMU.h"

// persistent state info to fetch a TLE from a remote web site incrementally
struct TLEFetch {
    bool running;      // set while reading a remote file
    char sat[30];     // scrubbed name of satellite we are looking for
    char buf[200];      // long enough for a complete name and TLE: ~30+70+70
    char *l0, *l1, *l2;     // start of each TLE line within buf[], l0==NULL until complete
    WiFiClient *remote;     // remote connection object
    int lineno;       // show line number as progress
};

class Webpage
{
  public:
    Webpage();
    void checkNetwork();

  private:
    static constexpr size_t user_message_size = 64;
    /* record a brief message to inform the user, it will be sent on the next sendNewValues() sweep.
     * the message consists of a string anda trailing character, typically
     *  '!' to indicate an alarm, '+' to indicate good progress, or '\0' for no effect.
     */
    char user_message[user_message_size];
    WiFiServer *httpServer;
    bool connectWiFi();

    char readNextClientChar (WiFiClient client, uint32_t *to);
    void scrub(char *s);

    void startTLEFetch(char *query_text);
    void resumeTLEFetch(void);
    TLEFetch tlef;

    void overrideValue(WiFiClient client);
    void sendNewValues(WiFiClient client);

    void sendMainPage(WiFiClient client);
    void sendPlainHeader(WiFiClient client);
    void sendHTMLHeader(WiFiClient client, bool compressed = false);
    void sendEmptyResponse(WiFiClient client);
    void send404Page(WiFiClient client);

    void reboot();
};

extern Webpage *webpage;

#endif // _WEBPAGE_H
