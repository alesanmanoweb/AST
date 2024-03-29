#ifndef _GPS_H
#define  _GPS_H

#include <Adafruit_GPS.h>
#include <WiFiClient.h>

#include "P13.h"
#include "Target.h"

extern int magdecl( double l, double L, double e, double y, double *mdp);

class GPS
{
  private:
    /* flags
    */
    bool gps_lock; // set when we get a valid fix
    bool gps_ok; // set when we get a valid line
    bool time_overridden; // some element of time has been set by op
    bool loc_overridden; // some element of location has been set by op

    Observer *obs; // topocentric place

    float decimalYear();
    void newObserver(float lat, float lng, float hgt);

    /* implement on top of DateTime a running time based on elapsed millis()
     */
    DateTime dt_now;
    float dt_TN0;
    long dt_DN0;
    uint32_t dt_m0;
    void getnow(int &year, uint8_t &month, uint8_t &day, uint8_t &h, uint8_t &m, uint8_t &s);
    void setnow(int year, uint8_t month, uint8_t day, uint8_t h, uint8_t m, uint8_t s);

  public:
    Adafruit_GPS *gps;
    double magdeclination; // true az - magnetic az
    float latitude, longitude; // degs +N, +E
    float altitude; // altitude above MSL, m
    float hdop; // horizontal degradation
    int nsats; // number of satellites used
  
    GPS();
    void sendNewValues(WiFiClient client);
    bool overrideValue(char *name, char *value);
    void checkGPS();
    DateTime now() {return dt_now;};
    float age(Satellite *sat);
    Observer *observer();
    void printSexa(WiFiClient client, float v);
    void printHMS(WiFiClient client, uint8_t h, uint8_t m, uint8_t s);
    void printDate(WiFiClient client, int y, uint8_t m, uint8_t d);
  
  enum PrintLevel
  {
    NORMAL, BADNEWS, GOODNEWS
  };
  void printPL(WiFiClient client, PrintLevel pl);

};

extern GPS *gps;

#endif // _GPS_H
