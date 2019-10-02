#include "Webpage.h"
#include "GPS.h"
#include "IMU.h"

Webpage *webpage;
GPS *gps;
IMU *imuA;
Target *target;

void setup()
{
  Serial.begin(115200);
  Serial.println("Making Webpage");
  webpage = new Webpage();
  Serial.println("Making GPS");
  gps = new GPS();
  Serial.println("Making IMU");
  imuA = new IMU();
  Serial.println("Making Target");
  target = new Target();
}

void loop()
{
  // check for ethernet activity
  webpage->checkNetwork();

  // check for new GPS info
  gps->checkGPS();

  // follow the target
  target->track();
}

// controllare logging, tutti i // e #if etc
// controllo motori
