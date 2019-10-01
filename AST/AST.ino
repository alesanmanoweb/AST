#include "Webpage.h"

Webpage *webpage;

void setup()
{
  Serial.begin(115200);
  Serial.println(F("Making Webpage"));
  webpage = new Webpage();
}

void loop()
{
  // check for ethernet activity
  webpage->checkNetwork();
}
