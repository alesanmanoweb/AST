// class to control two motors to track a target az and el using the Adafruit I2C interface

#ifndef _ROTATOR_H
#define _ROTATOR_H

#include <WiFiClient.h>

#include "Target.h"
#include "IMU.h"

#define ENA 14
#define PWMA 1
#define Ai1 32
#define Ai2 15

#define ENB 33
#define PWMB 0
#define Bi1 27
#define Bi2 21

#define duty 180

struct motor_interface
{
  int drive;
  int A;
  int B;
};

class Rotator
{
  private:
    static constexpr int PWM_FREQ = 5000;
    static constexpr int PWM_DUTY = 180;
    static constexpr size_t NMOTORS = 2; // not easily changed
    motor_interface motor[NMOTORS];

    void azimuthLeft();
    void azimuthRight();
    void azimuthMove(int d);

    void elevationUp();
    void elevationDown();
    void elevationMove(int d);

    bool moving_right;
    // search info
//    void calibrate(float &az_s, float &el_s);
//    void seekTarget(float& az_t, float& el_t, float& az_s, float& el_s);
//    float azDist(float &from, float &to);

  public:
    Rotator();
    void moveToAzEl(float az_t, float el_t);
    void sendNewValues(WiFiClient client);
    bool overrideValue(char *name, char *value);
//    bool connected() { return true; };
//    bool calibrated() { return (init_step >= N_INIT_STEPS); }
};

extern Rotator *rotator;

#endif // _ROTATOR_H
