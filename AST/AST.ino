#include "Webpage.h"
#include "GPS.h"
#include "IMU.h"

Webpage *webpage;
GPS *gps;
IMU *imuA;

void setup()
{
  Serial.begin(115200);
  Serial.println("Making Webpage");
  webpage = new Webpage();
  Serial.println("Making GPS");
  gps = new GPS();
  Serial.println("Making IMU");
  imuA = new IMU();
}

void loop()
{
  // check for ethernet activity
  webpage->checkNetwork();

  // check for new GPS info
  gps->checkGPS();
}

// leggere sensore 9DOF

// calcolo traiettorie
// controllo motori
