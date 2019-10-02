/* this class contains information about the 9 dof spatial sensor
 */

#include <Wire.h>
#include <Preferences.h>

#include "IMU.h"

/* class constructor
 */
IMU::IMU()
{
  // instantiate, discover and initialize
  bno = new Adafruit_BNO055();
  sensor_found = bno->begin(Adafruit_BNO055::OPERATION_MODE_NDOF);
  if(sensor_found)
  {
    Serial.println("IMU found ok");
  }
  else
  {
    Serial.println("Sensor not found");
  }
  installCalibration();
}

/* read the current temperature, in degrees C
 */
int8_t IMU::getTempC()
{
  if(sensor_found)
  {
    return bno->getTemp();
  }
  return -1;
}

/* return whether sensor is connected and calibrated
 */
bool IMU::calibrated(uint8_t &sys, uint8_t &gyro, uint8_t &accel, uint8_t &mag)
{
  if(!sensor_found)
  {
    return (false);
  }
  sys = 0;
  gyro = 0;
  accel = 0;
  mag = 0;;
  bno->getCalibration(&sys, &gyro, &accel, &mag);
  return sys >= 1 && gyro >= 1 && accel >= 1 && mag >= 1;
}

/* read the current az and el, corrected for mag decl but not necessarily calibrated.
 * N.B. we assume this will only be called if we know the sensor is connected.
 * N.B. Adafruit board:
 *   the short dimension is parallel to the antenna boom,
 *   the populated side of the board faces upwards and
 *   the side with the control signals (SDA, SCL etc) points in the rear direction of the antenna pattern.
 * Note that az/el is a left-hand coordinate system.
 */
void IMU::getAzEl(float *azp, float *elp)
{
  imu::Vector<3> euler = bno->getVector(Adafruit_BNO055::VECTOR_EULER);
  *azp = fmod(euler.x() + gps->magdeclination + 540, 360);
  *elp = euler.y();
}

/* process name = value pair
 * return whether we recognize it
 */
bool IMU::overrideValue(char *name, char *value)
{
  if(!strcmp(name, "SS_Save"))
  {
    saveCalibration();
    //webpage->setUserMessage("Sensor calibrations saved to EEPROM+");
    return true;
  }
  return false;
}

/* send latest values to web page
 * N.B. labels must match ids in wab page
 */
void IMU::sendNewValues(WiFiClient client)
{
  if(!sensor_found)
  {
      client.println("SS_Status=Not found!");
      client.println("SS_Save=false");
      return;
  }

  float az, el;
  getAzEl(&az, &el);
  client.print("SS_Az=");
  client.println(az);
  client.print("SS_El=");
  client.println(el);

  uint8_t sys, gyro, accel, mag;
  bool calok = calibrated (sys, gyro, accel, mag);
  if (calok)
  {
    client.println("SS_Status=Ok+");
  }
  else
  {
    client.println("SS_Status=Uncalibrated!");
  }
  client.print("SS_SStatus=");
  client.println(sys);
  client.print("SS_GStatus=");
  client.println(gyro);
  client.print("SS_MStatus=");
  client.println(mag);
  client.print("SS_AStatus=");
  client.println(accel);

  client.print("SS_Save=");
  if(calok && sys == 3 && gyro == 3 && accel == 3 && mag == 3)
  {
    client.println("true");
  }
  else
  {
    client.println("false");
  }

  client.print("SS_Temp=");
  client.println(getTempC());
}

/* read the sensor calibration values and save into EEPROM.
 * Wanted to stick with stock Adafruit lib so pulled from
 * post by protonstorm at https://forums.adafruit.com/viewtopic.php?f=19&t=75497
 */
void IMU::saveCalibration()
{
  Preferences preferences;
  preferences.begin("AST", false);
  // put into config mode
  bno->setMode(Adafruit_BNO055::OPERATION_MODE_CONFIG);
  delay(25);

  // request all bytes starting with the ACCEL
  byte cal_data[NBNO055CALBYTES];
  Wire.beginTransmission((uint8_t)BNO055_I2CADDR);
  Wire.write((uint8_t)(Adafruit_BNO055::ACCEL_OFFSET_X_LSB_ADDR));
  Wire.endTransmission();
  Wire.requestFrom(BNO055_I2CADDR, NBNO055CALBYTES);

  // copy to NV
  Serial.println("Saving sensor values");
  for(uint8_t i = 0; i < NBNO055CALBYTES; i++)
  {
    cal_data[i] = Wire.read();
    Serial.println(cal_data[i]);
  }
  preferences.putBytes("BNO055cal", cal_data, NBNO055CALBYTES);

  // restore NDOF mode
  bno->setMode(Adafruit_BNO055::OPERATION_MODE_NDOF);
  delay(25);

  // save in EEPROM
  preferences.end();
}

/* install previously stored calibration data from EEPROM if it looks valid.
 * Wanted to stick with stock Adafruit lib so pulled from
 * post by protonstorm at https://forums.adafruit.com/viewtopic.php?f=19&t=75497
 */
void IMU::installCalibration()
{
  Preferences preferences;
  preferences.begin("AST", true);
  
  byte cal_data[NBNO055CALBYTES];

  // read from EEPROM, qualify
  preferences.getBytes("BNO055cal", cal_data, NBNO055CALBYTES);
  preferences.end();

  int i;
  for(i = 0; i < NBNO055CALBYTES; i++)
  {
    if(cal_data[i] != 0)
    {
      break;
    }
  }
  if(i == NBNO055CALBYTES)
  {
    return; // all zeros can't be valid
  }

  // put into config mode
  bno->setMode(Adafruit_BNO055::OPERATION_MODE_CONFIG);
  delay(25);

  // Serial.println (F("Restoring sensor values"));
  for(i = 0; i < NBNO055CALBYTES; i++)
  {
    Wire.beginTransmission((uint8_t)BNO055_I2CADDR);
    Wire.write((Adafruit_BNO055::adafruit_bno055_reg_t)((uint8_t)(Adafruit_BNO055::ACCEL_OFFSET_X_LSB_ADDR) + i));
    Wire.write(cal_data[i]);
    // Serial.println (nv->BNO055cal[i]);
    Wire.endTransmission();
  }

  // restore NDOF mode
  bno->setMode(Adafruit_BNO055::OPERATION_MODE_NDOF);
  delay(25);
}
