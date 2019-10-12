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
  digitalWrite(Ai1, LOW);
  digitalWrite(Ai2, HIGH);
}

void Rotator::azimuthRight()
{
  digitalWrite(Ai2, LOW);
  digitalWrite(Ai1, HIGH);
}

void Rotator::azimuthMove(int diff)
{
  //int duty = (pwm_max_duty_az - pwm_min_duty_az) * () + pwm_min_duty_az;
  int duty;
  if(diff > 45)
  {
    duty = pwm_max_duty_az;
  }
  else
  {
    duty = (pwm_max_duty_az - pwm_min_duty_az) * diff / 45 + pwm_min_duty_az;
  }
  
  ledcWrite(PWMA, duty);
}
  
void Rotator::azimuthStop()
{
  ledcWrite(PWMA, 0);
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

void Rotator::elevationMove(int diff)
{
  int duty;
  if(diff > 45)
  {
    duty = pwm_max_duty_el;
  }
  else
  {
    duty = (pwm_max_duty_el - pwm_min_duty_el) * diff / 45 + pwm_min_duty_el;
  }
  
  ledcWrite(PWMB, duty);
}

void Rotator::elevationStop()
{
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

  bool el_ok = false;
  bool az_ok = false;

  while(!el_ok || !az_ok)
  {
    imuA->getAzEl(&x, &y);

    int diff_y = abs(target_y - y);
    if( diff_y >= precision)
    {
      //Serial.printf("Adjusting elevation: Y=%d; T_Y=%d; count=%d\n", (int)y, target_y, count);
      if(y < target_y)
      {
        // go up
        elevationUp();
      }
      else if(y > target_y)
      {
        // go down
        elevationDown();
      }
      elevationMove(diff_y);
      el_ok = false;
    }
    else
    {
      elevationStop();
      el_ok = true;
    }


    if(abs(target_x - x) >= precision && (target_x + 360 - x) >= precision)
    {
      //Serial.printf("Adjusting azimuth    X=%03d; T_X=%d; count=%d\n", (int)x, target_x, count);
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
      }
      else
      {
        // go right, decrease x
        azimuthRight();
      }
      azimuthMove(abs(a - b));
      az_ok = false;
    }
    else
    {
      azimuthStop();
      az_ok = true;
    }

    if(count++ >= 200)
    {
      Serial.println("Count break!!!!!!!!!!!!");
      break;
    }
  }
  Serial.printf("Count=%d\n", count);
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
      elevationMove(45);
      delay(100);
      elevationStop();
    }
    else if(!strcmp(value, "Down"))
    {
      elevationDown();
      elevationMove(15);
      delay(100);
      elevationStop();
    }
    else if(!strcmp(value, "Left"))
    {
      azimuthLeft();
      azimuthMove(15);
      delay(100);
      azimuthStop();
    }
    else if(!strcmp(value, "Right"))
    {
      azimuthRight();
      azimuthMove(45);
      delay(100);
      azimuthStop();
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
