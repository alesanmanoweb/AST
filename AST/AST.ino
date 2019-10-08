#include "Webpage.h"
#include "GPS.h"
#include "IMU.h"

Webpage *webpage;
GPS *gps;
IMU *imuA;
Target *target;
Rotator *rotator;

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
  Serial.println("Making Rotator");
  rotator = new Rotator();
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
