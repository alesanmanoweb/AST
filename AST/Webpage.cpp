#include "Webpage.h"
#include "html.h"

Webpage::Webpage()
{
  while(!connectWiFi())
  {
    Serial.println("Failed to setup Wifi. Retrying...");
  }

  // create server
  Serial.println("Creating http server");
  httpServer = new WiFiServer(80);
  httpServer->begin();
  Serial.println(WiFi.localIP());
}

/* call this occasionally to check for Ethernet activity
 */
void Webpage::checkNetwork()
{
  WiFiClient client = httpServer->available();
  if(!client)
  {
    return;
  }

  Serial.println("Client connected");
  uint32_t t0 = millis(); // init timeout
  char first_header_line[128]; // first header line (we discard the rest)
  unsigned fhll = 0; // first header line length
  bool eoh = false; // set when see end of header
  bool fldone = false; // set when finished collecting first header line
  char c, prevc = 0; // new and previous character read from client

  // read header, collecting first line and discarding rest until find blank line
  while(!eoh && (c = readNextClientChar(client, &t0)))
  {
    if (c == '\n')
    {
      if(!fldone)
      {
          first_header_line[fhll] = '\0';
          fldone = true;
      }
      if (prevc == '\n')
      {
          eoh = true;
      }
    }
    else if(!fldone && fhll < sizeof(first_header_line)-1)
    {
      first_header_line[fhll++] = c;
    }
    prevc = c;
  }
  if (c == 0)
  {
    // Serial.println ("closing client");
    client.stop();
    return;
  }

  // client socket is now at first character after blank line

  // replace trailing ?time cache-buster with blank
  char *q = strrchr(first_header_line, '?');
  if(q)
  {
    *q = ' ';
  }

  // what we do next depends on first line
  // Serial.println (first_header_line);
  if(strstr(first_header_line, "GET / "))
  {
    sendMainPage(client);
  }
  else if(strstr(first_header_line, "GET /getvalues.txt "))
  {
//    sendNewValues(client);
  }
  else if(strstr(first_header_line, "POST / "))
  {
//    overrideValue(client);
    sendEmptyResponse(client);
  }
  else if(strstr(first_header_line, "POST /reboot "))
  {
    sendEmptyResponse(client);
    reboot();
  }
  else if(strstr(first_header_line, "POST /start "))
  {
//    target->setTrackingState(true);
    sendEmptyResponse(client);
  }
  else if(strstr(first_header_line, "POST /stop "))
  {
//    target->setTrackingState(false);
    sendEmptyResponse(client);
  }
  else
  {
    send404Page(client);
  }

  // finished
  // Serial.println ("closing client");
  client.stop();
}

/* try to connect to wifi using creds we have in EEPROM
 * return whether it connected ok
 */
bool Webpage::connectWiFi()
{
  // start over
  // WiFi.disconnect(true);
  // WiFi.softAPdisconnect(true);
  delay(400);

  // configure
  //WiFi.mode(WIFI_STA);
  WiFi.begin("as2", "bymoonlightweridetenthousandssidebyside");
  //WiFi.config(nv->IP, nv->GW, nv->NM);

  // wait for connect
  uint32_t t0 = millis();
  uint32_t timeout = 15000;
  while(WiFi.status() != WL_CONNECTED)
  {
    if (millis() - t0 > timeout)
    {
      Serial.println (F("Unable to connect to AP"));
      return false;
    }
    delay(100);
  }
  Serial.println("WiFi Connection established!");  
  return true;
}

/* read next character, return 0 if it disconnects or times out.
 * '\r' is discarded completely.
 */
char Webpage::readNextClientChar(WiFiClient client, uint32_t *t0)
{
  static const int timeout = 1000; // client socket timeout, ms
  while(client.connected())
  {
    if(millis() > *t0 + timeout)
    {
      Serial.println ("Client timed out");
      return 0;
    }
    if(!client.available())
    {
      continue;
    }
    char c = client.read();
    *t0 = millis();
    if (c == '\r')
    {
      continue;
    }
    // Serial.write(c);
    return (c);
  }
  // Serial.println ("client disconnected");
  return (0);
}

/* remove non-alphanumeric chars and convert to upper case IN PLACE
 */
void Webpage::scrub(char *s)
{
  char *scrub_s;
  for(scrub_s = s; *s != '\0'; s++)
  {
    if(isalnum(*s))
      *scrub_s++ = toupper(*s);
  }
  *scrub_s = '\0';
}

/* op has entered manually a value to be overridden.
 * client is at beginning of NAME=VALUE line, parse and send to each subsystem
 * N.B. a few are treated specially.
 */
void Webpage::overrideValue(WiFiClient client)
{
#if 0
  char c, buf[200]; // must be at least enough for a known-valid TLE
  uint8_t nbuf = 0; // type must be large enough to count to sizeof(buf)

  // read next line into buf
  uint32_t t0 = millis(); // init timeout
  while((c = readNextClientChar(client, &t0)) != 0)
  {
    if (c == '\n')
    {
      buf[nbuf++] = '\0';
      break;
    }
    else if(nbuf < sizeof(buf)-1)
    {
      buf[nbuf++] = c;
    }
  }
  if(c == 0)
  {
    return; // bogus; let caller close
  }

  // break at = into name, value
  char *valu = strchr(buf, '=');
  if(!valu)
  {
      return; // bogus; let caller close
  }
  *valu++ = '\0'; // replace = with 0 then valu starts at next char
  // now buf is NAME and valu is VALUE

  Serial.print(F("Override: "));
  Serial.print(buf);
  Serial.print("=");
  Serial.println(valu);

  if(strcmp (buf, "T_TLE") == 0)
  {
    // T_TLE needs two more lines
    char *l1 = valu;    // TLE target name is valu
    char *l2 = &buf[nbuf];  // line 2 begins after valu
    char *l3 = NULL;    // set when find end of line 2

    // scan for two more lines
    uint8_t nlines = 1;
    while(nlines < 3 && (c = readNextClientChar(client, &to)) != 0)
    {
      if (c == '\n')
      {
        buf[nbuf++] = '\0';
        if (++nlines == 2)
        {
          l3 = &buf[nbuf];  // line 3 starts next
        }
      }
      else if(nbuf < sizeof(buf)-1)
      {
        buf[nbuf++] = c;
      }
    }
    if (nlines < 3)
    {
      return; // premature end, let caller close
    }

    // new target!
    target->setTLE(l1, l2, l3);
  }
  else if(strcmp (buf, "IP") == 0)
  {
    // op is setting a new IP, save in EEPROM for use on next reboot
    char *octet = valu;
    for (uint8_t i = 0; i < 4; i++)
    {
      int o = atoi(octet);
      if(o < 0 || o > 255)
      {
        return; // bogus IP
      }
      nv->IP[i] = o;
      if (i == 3)
      {
        break;
      }
      octet = strchr(octet, '.'); // find next .
      if(!octet)
      {
        return;       // bogus format
      }
      octet++;        // point to first char after .
    }
    nv->put();
    setUserMessage (F("Successfully stored new IP address in EEPROM -- reboot to engage+"));
  }
  else if(strcmp (buf, "querySite") == 0)
  {
    // op wants to look up a target at a web site, valu is target,url
    startTLEFetch (valu);
  }
  else
  {
    // not ours, give to each other subsystem in turn until one accepts
    if (!circum->overrideValue (buf, valu)
    && !gimbal->overrideValue (buf, valu)
    && !target->overrideValue (buf, valu)
    && !sensor->overrideValue (buf, valu))
    setUserMessage (F("Bug: unknown override -- see Serial Monitor!"));
  }
#endif
}

/* inform each subsystem to send its latest values, including ours
 */
void Webpage::sendNewValues(WiFiClient client)
{
#if 0
  // send plain text header for NAME=VALUE pairs
  sendPlainHeader(client);

  // send user message
  client.print("op_message=");
  if(user_message_F != NULL)
  {
    client.print(user_message_F);
  }
  if(user_message_s[0])
  {
    client.print(user_message_s);
  }
  client.println();

  // send our values
  client.print("IP=");
  for(uint8_t i = 0; i < 4; i++)
  {
    client.print(nv->IP[i]);
    if(i < 3)
    {
      client.print(F("."));
    }
  }
  client.println(F(""));

  if (tlef.l0) {
      // set newly fetched name on web page
      client.print (F("new_TLE="));
      client.println (tlef.l0);
      client.println (tlef.l1);
      client.println (tlef.l2);
      tlef.l0 = NULL;     // just send once
  }

  client.print (F("uptime="));
  circum->printSexa (client, millis()/1000.0/3600.0);
  circum->printPL (client, Circum::NORMAL);

  // send whatever the other modules want to
  circum->sendNewValues (client);
  gimbal->sendNewValues (client);
  sensor->sendNewValues (client);
  target->sendNewValues (client);
#endif
}

/* send the main page, in turn it will send us commands using XMLHttpRequest
 */

 
void Webpage::sendMainPage(WiFiClient client)
{
  sendCompressedHTMLHeader(client);
  client.write(html, sizeof(html));
}

/* send HTTP header for plain text content
 */
void Webpage::sendPlainHeader(WiFiClient client)
{
  client.print(F(
      "HTTP/1.0 200 OK \r\n"
      "Content-Type: text/plain \r\n"
      "Connection: close \r\n"
      "\r\n"
  ));
}

/* send HTTP header for html content
 */
void Webpage::sendHTMLHeader(WiFiClient client)
{
  client.print(F(
      "HTTP/1.0 200 OK \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "\r\n"
  ));
}

/* send HTTP header for html content
 */
void Webpage::sendCompressedHTMLHeader(WiFiClient client)
{
  client.print(F(
      "HTTP/1.0 200 OK \r\n"
      "Content-Encoding: gzip \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "\r\n"
  ));
}

/* send empty response
 */
void Webpage::sendEmptyResponse(WiFiClient client)
{
  client.print(F(
      "HTTP/1.0 200 OK \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "Content-Length: 0 \r\n"
      "\r\n"
  ));
}

/* send back error 404 when requested page not found.
 * N.B. important for chrome otherwise it keeps asking for favicon.ico
 */
void Webpage::send404Page(WiFiClient client)
{
  Serial.println("Sending 404");
  client.print(F(
      "HTTP/1.0 404 Not Found \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "\r\n"
      "<html> \r\n"
      "<body> \r\n"
      "<h2>404: Not found</h2>\r\n \r\n"
      "</body> \r\n"
      "</html> \r\n"
  ));
}

/* reboot
 */
void Webpage::reboot()
{
  Serial.println("rebooting");
  delay(500);
  ESP.restart();
}
