/* this class contains information about the 9 dof spatial sensor
 */

#ifndef _IMU_H
#define _IMU_H

#include <WiFiClient.h>
#include <Adafruit_BNO055.h>

#define BNO055_I2CADDR 0x28
#define NBNO055CALBYTES 22

#include "GPS.h"

class IMU
{
  private:
    Adafruit_BNO055 *bno; // sensor detail
    bool sensor_found; // whether sensor is connected
    bool calibrated(uint8_t &sys, uint8_t &gyro, uint8_t &accel, uint8_t &mag);
  
  public:
    IMU();
    int8_t getTempC();
    void getAzEl(float *azp, float *elp);
    void sendNewValues(WiFiClient client);
    bool connected() { return sensor_found; };
    void saveCalibration(void);
    void installCalibration(void);
    bool overrideValue(char *name, char *value);
};

extern IMU *imuA;

#endif // _IMU_H
