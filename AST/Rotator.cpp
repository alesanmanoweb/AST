// class to control two DC motors to track a target az and el using external H-bridges

#include "Rotator.h"
#include "IMU.h"

Rotator::Rotator()
{
  precision = 5;
  
  pinMode(Ai1, OUTPUT);
  pinMode(Ai2, OUTPUT);
  digitalWrite(Ai1, LOW);
  digitalWrite(Ai2, HIGH);

  ledcSetup(PWMA, 20000, 8);
  ledcAttachPin(ENA, PWMA);
  ledcWrite(PWMA, 0);

  pinMode(Bi1, OUTPUT);
  pinMode(Bi2, OUTPUT);
  digitalWrite(Bi1, LOW);
  digitalWrite(Bi2, HIGH);
  
  ledcSetup(PWMB, 20000, 8);
  ledcAttachPin(ENB, PWMB);
  ledcWrite(PWMB, 0);

  azimuthRight();
  elevationUp();
  Serial.println("Rotator created");
}

void Rotator::azimuthLeft()
{
  moving_right = false;
  digitalWrite(Ai1, LOW);
  digitalWrite(Ai2, HIGH);
}

void Rotator::azimuthRight()
{
  moving_right = true;
  digitalWrite(Ai2, LOW);
  digitalWrite(Ai1, HIGH);
}

void Rotator::azimuthMove(int d)
{
  ledcWrite(PWMA, duty);
  if(moving_right)
    elevationDown();
  else
    elevationUp();
  ledcWrite(PWMB, duty);//*1000/960);
  delay(d);
  ledcWrite(PWMA, 0);
  ledcWrite(PWMB, 0);
}
  
void Rotator::elevationDown()
{
  digitalWrite(Bi1, LOW);
  digitalWrite(Bi2, HIGH);
}

void Rotator::elevationUp()
{
  digitalWrite(Bi2, LOW);
  digitalWrite(Bi1, HIGH);
}

void Rotator::elevationMove(int d)
{
  ledcWrite(PWMB, duty);
  delay(d);
  ledcWrite(PWMB, 0);
}

void Rotator::moveToAzEl(float az_t, float el_t)
{
  //Serial.printf("Rotator move to: %f, %f\n", az_t, el_t);

  float x, y;
  int target_x, target_y;
  target_x = az_t;
  target_y = el_t;
  int count = 0;
  imuA->getAzEl(&x, &y);
  while(abs(target_y - y) >= precision)
  {
    Serial.printf("Adjusting elevation: Y=%d; T_Y=%d; count=%d\n", (int)y, target_y, count);
    if(y < target_y)
    {
      // go up
      elevationUp();
      elevationMove(15);
    }
    else if(y > target_y)
    {
      // go down
      elevationDown();
      elevationMove(15);
    }
    if(count++ >= 20)
      break;
    delay(100);
    imuA->getAzEl(&x, &y);
  }

  count = 0;
  while(abs(target_x - x) >= precision && (target_x + 360 - x) >= precision)
  {
    Serial.printf("X=%03d; T_X=%d; count=%d\n", (int)x, target_x, count);
    int a = x;
    int b = target_x;
    if(abs(a-b) > 180)
    {
      int temp = a;
      a = b;
      b = temp;
    }
    if(a < b)
    {
      // go left, increase x
      azimuthLeft();
      azimuthMove(15);
    }
    else
    {
      // go right, decrease x
      azimuthRight();
      azimuthMove(15);
    }
    if(count++ >= 20)
      break;
    delay(100);
    imuA->getAzEl(&x, &y);
  }
}

void Rotator::sendNewValues(WiFiClient client)
{
  client.print("R_Precision=");
  client.println(precision);
}

bool Rotator::overrideValue(char *name, char *value)
{
  // precisione
  // duty
  Serial.printf("ROTATOR: Name:%s Value:%s\n",name, value);
  if(!strcmp(name, "R_Move"))
  {
    if(!strcmp(value, "Up"))
    {
      elevationUp();
      elevationMove(100);
    }
    else if(!strcmp(value, "Down"))
    {
      elevationDown();
      elevationMove(100);
    }
    else if(!strcmp(value, "Left"))
    {
      azimuthLeft();
      azimuthMove(100);
    }
    else if(!strcmp(value, "Right"))
    {
      azimuthRight();
      azimuthMove(100);
    }
    return true;
  }
  else if(!strcmp(name, "R_Precision"))
  {
    int new_precision = atoi(value);
    if(new_precision > 0 && new_precision < 90)
    {
      precision = new_precision;
    }
    Serial.printf("New precision: %d\n", precision);
    return true;
  }
  return false;
}
