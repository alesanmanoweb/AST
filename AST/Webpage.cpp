#include <Preferences.h>

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

  setUserMessage("Hello+");

  memset(&tlef, 0, sizeof(tlef));
  tlef.running = false;
}

/* call this occasionally to check for Ethernet activity
 */
void Webpage::checkNetwork()
{
  // do more of remote fetch if active
  resumeTLEFetch();
  
  WiFiClient client = httpServer->available();
  if(!client)
  {
    return;
  }

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
    sendNewValues(client);
  }
  else if(strstr(first_header_line, "POST / "))
  {
    overrideValue(client);
    sendEmptyResponse(client);
  }
  else if(strstr(first_header_line, "POST /reboot "))
  {
    sendEmptyResponse(client);
    reboot();
  }
  else if(strstr(first_header_line, "POST /start "))
  {
    target->setTrackingState(true);
    sendEmptyResponse(client);
  }
  else if(strstr(first_header_line, "POST /stop "))
  {
    target->setTrackingState(false);
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

void Webpage::setUserMessage(const char* msg, const char* value, char status)
{
  snprintf(user_message, user_message_size, "%s%s%c", msg, value, status);
}

void Webpage::setUserMessage(const char* msg, int value, char status)
{
  snprintf(user_message, user_message_size, "%s%d%c", msg, value, status);
}

/* try to connect to wifi using creds we have in EEPROM
 * return whether it connected ok
 */
bool Webpage::connectWiFi()
{
  Preferences preferences;
  preferences.begin("AST", true);
  // configure
  //WiFi.mode(WIFI_STA);
  WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PSK").c_str());
  //WiFi.config(nv->IP, nv->GW, nv->NM);
  preferences.end();

  // wait for connect
  uint32_t t0 = millis();
  uint32_t timeout = 15000;
  while(WiFi.status() != WL_CONNECTED)
  {
    if (millis() - t0 > timeout)
    {
      Serial.println ("Unable to connect to AP");
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
    {
      *scrub_s++ = toupper(*s);
    }
  }
  *scrub_s = '\0';
}

/* given "sat,URL" search the given URL for the given satellite TLE.
 * N.B. in order for our web page to continue to function, this method is just the first step, other
 * steps are done incrementally by resumeTLEFetch().
 */
void Webpage::startTLEFetch(char *query_text)
{
  // split query at ',' to get sat name and URL
  char *sat = query_text;
  char *url = strchr(query_text, ',');
  if(!url)
  {
    setUserMessage("Invalid querySite string: ", query_text, '!');
    return;
  }
  *url++ = '\0'; // overwrite ',' with EOS for sat then move to start of url

  // remove leading protocol, if any
  if(strncmp(url, "http://", 7) == 0)
  {
    url += 7;
  }

  // file name
  char *path = strchr(url, '/');
  if(!path)
  {
    setUserMessage("Invalid querySite URL: ", url, '!');
    return;
  }
  *path++ = '\0'; // overwrite '/' with EOS for server then move to start of path

  // connect
  tlef.remote = new WiFiClient();
  if(!tlef.remote->connect(url, 80))
  {
    setUserMessage("Failed to connect to ", url, '!');
    delete tlef.remote;
    return;
  }

  // send query to retrieve the file containing TLEs
  // Serial.print(sat); Serial.print(F("@")); Serial.println (url);
  tlef.remote->print("GET /");
  tlef.remote->print(path);
  tlef.remote->print(" HTTP/1.0\r\nContent-Type: text/plain \r\n\r\n");

  // set up so we can resume the search later....
  scrub(sat);
  strncpy(tlef.sat, sat, sizeof(tlef.sat)-1);
  tlef.lineno = 1;
  tlef.running = true;
}

/* called to resume fetching a remote web page, started by startTLEFetch().
 * we are called periodically regardless, do nothing if no fetch is in progress.
 * we know to run based on whether tlef.remote is connected.
 */

void Webpage::resumeTLEFetch()
{
  // skip if nothing in progress
  if(!tlef.running)
  {
    return;
  }

#if 0
while(tlef.remote->available())
  {
    char c = tlef.remote->read();
    Serial.write(c);
  }

  if(!tlef.remote->connected())
  {
    tlef.remote->stop();
    delete tlef.remote;
    tlef.running = false;
  }
return;
#endif
  // init
  const uint32_t tout = millis() + 10000; // timeout, ms
  uint8_t nfound = 0; // n good lines found so far
  char *bp = tlef.buf; // next buf position to use
  tlef.l0 = NULL; // flag for sendNewValues();

  // read another line, if find sat read two more and finish up
  while(tlef.remote->connected() && nfound < 3 && millis() < tout)
  {
    if(tlef.remote->available())
    {
      char c = tlef.remote->read();
      if(c == '\r')
      {
        continue;
      }
      if(c == '\n')
      {
        // show some progress
        tlef.lineno++;
        setUserMessage("Reading line ", tlef.lineno, '+');

        *bp++ = '\0';
        switch(nfound)
        {
          case 0:
            char sl0[50]; // copy enough that surely contains sat
            strncpy(sl0, tlef.buf, sizeof(sl0)-1);
            sl0[sizeof(sl0)-1] = '\0'; // insure EOS
            scrub(sl0); // scrub the copy so l0 remains complete
            if(strstr(sl0, tlef.sat)) // look for scrubbed sat in scrubbed l0
            {
              // found sat, prepare to collect TLE line 1 in l1
              nfound++; // found name
              tlef.l0 = tlef.buf; // l0 begins at buf
              tlef.l1 = bp; // l1 begins at next buf position
            }
            else
            {
              return; // try next line on next call
            }
            break;
          case 1:
            if(target->tleValidChecksum(tlef.l1))
            {
              // found TLE line 1, prep for line 2
              nfound++;     // found l1
              tlef.l2 = bp;   // l2 begins at next buf position
            }
            else
            {
              nfound = 0;     // no good afterall
              tlef.l0 = NULL;   // reset flag for sendNewValues()
            }
            break;
          case 2:
            if(target->tleValidChecksum(tlef.l2)) // found last line
            {
              nfound++; // found l2
            }
            else
            {
              nfound = 0;     // no good afterall
              tlef.l0 = NULL;   // reset flag for sendNewValues()
            }
            break;
          default:
            // can't happen ;-)
            break;
        }
      }
      else if(bp - tlef.buf < (int)(sizeof(tlef.buf)-1))
      {
        *bp++ = c;        // add to buf iif room, including EOS
      }
    }
    else
    {
    // static long n;
    // Serial.println (n++);
    }
  }

  // get here if remote disconnected, found sat or timed out
  if(!tlef.remote->connected())
  {
    setUserMessage("TLE not found!");
  }
  else if(nfound == 3)
  {
    setUserMessage("Found TLE: ", tlef.l0, '+');
  }
  else
  {
    setUserMessage("Remote site timed out!");
  }

  // finished regardless
  tlef.remote->stop();
  delete tlef.remote;
  tlef.running = false;
}

/* op has entered manually a value to be overridden.
 * client is at beginning of NAME=VALUE line, parse and send to each subsystem
 * N.B. a few are treated specially.
 */
void Webpage::overrideValue(WiFiClient client)
{
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

  Serial.print("Override: ");
  Serial.print(buf);
  Serial.print("=");
  Serial.println(valu);

  if(strcmp (buf, "T_TLE") == 0)
  {
    // T_TLE needs two more lines
    char *l1 = valu; // TLE target name is valu
    char *l2 = &buf[nbuf]; // line 2 begins after valu
    char *l3 = NULL; // set when find end of line 2

    // scan for two more lines
    uint8_t nlines = 1;
    while(nlines < 3 && (c = readNextClientChar(client, &t0)) != 0)
    {
      if (c == '\n')
      {
        buf[nbuf++] = '\0';
        if (++nlines == 2)
        {
          l3 = &buf[nbuf];  // line 3 starts next
        }
      }
      else if(nbuf < sizeof(buf) - 1)
      {
        buf[nbuf++] = c;
      }
    }
    if(nlines < 3)
    {
      return; // premature end, let caller close
    }

    // new target!
    target->setTLE(l1, l2, l3);
  }
#if 0
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
#endif
  else if(strcmp (buf, "querySite") == 0)
  {
    // op wants to look up a target at a web site, valu is target,url
    startTLEFetch(valu);
  }
  else
  {
    // not ours, give to each other subsystem in turn until one accepts
    if(!gps->overrideValue(buf, valu) && !rotator->overrideValue(buf, valu) && !target->overrideValue(buf, valu) && !imuA->overrideValue(buf, valu))
    {
      setUserMessage("Bug: unknown override -- see Serial Monitor!");
    }
  }
}

/* inform each subsystem to send its latest values, including ours
 */
void Webpage::sendNewValues(WiFiClient client)
{
  // send plain text header for NAME=VALUE pairs
  sendPlainHeader(client);
  client.println("IP=" + WiFi.localIP().toString());
  client.print("Sys_freemem=");
  client.println(123);
  client.print("Sys_stack=");
  client.println(456);
  client.print("Sys_heap=");
  client.print(ESP.getFreeHeap());
  client.print(" / ");
  client.println(ESP.getHeapSize());

  // send user message
  client.print("op_message=");
  client.println(user_message);
  
  if(tlef.l0)
  {
    // set newly fetched name on web page
    client.print("new_TLE=");
    client.println(tlef.l0);
    client.println(tlef.l1);
    client.println(tlef.l2);
    tlef.l0 = NULL; // just send once
  }

  client.print("uptime=");
  gps->printSexa(client, millis()/1000.0/3600.0);
  gps->printPL(client, GPS::NORMAL);

  // send whatever the other modules want to
  gps->sendNewValues(client);
  rotator->sendNewValues(client);
  imuA->sendNewValues(client);
  target->sendNewValues(client);
}

/* send the main page, in turn it will send us commands using XMLHttpRequest
 */
void Webpage::sendMainPage(WiFiClient client)
{
  sendHTMLHeader(client, true);
  client.write(html, sizeof(html));
}

/* send HTTP header for plain text content
 */
void Webpage::sendPlainHeader(WiFiClient client)
{
  client.print(
      "HTTP/1.0 200 OK \r\n"
      "Content-Type: text/plain \r\n"
      "Connection: close \r\n"
      "\r\n"
  );
}

/* send HTTP header for html content
 */
void Webpage::sendHTMLHeader(WiFiClient client, bool compressed)
{
  client.print("HTTP/1.0 200 OK \r\n");
  if(compressed)
  {
    client.print("Content-Encoding: gzip \r\n");
  }
  client.print("Content-Type: text/html \r\nConnection: close \r\n\r\n");
}

/* send empty response
 */
void Webpage::sendEmptyResponse(WiFiClient client)
{
  client.print(
      "HTTP/1.0 200 OK \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "Content-Length: 0 \r\n"
      "\r\n"
  );
}

/* send back error 404 when requested page not found.
 * N.B. important for chrome otherwise it keeps asking for favicon.ico
 */
void Webpage::send404Page(WiFiClient client)
{
  Serial.println("Sending 404");
  client.print(
      "HTTP/1.0 404 Not Found \r\n"
      "Content-Type: text/html \r\n"
      "Connection: close \r\n"
      "\r\n"
      "<html> \r\n"
      "<body> \r\n"
      "<h2>404: Not found</h2>\r\n \r\n"
      "</body> \r\n"
      "</html> \r\n"
  );
}

/* reboot
 */
void Webpage::reboot()
{
  Serial.println("rebooting");
  delay(500);
  ESP.restart();
}
