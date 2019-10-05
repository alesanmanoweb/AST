/* define and track a target
 */

#include "Target.h"

/* constructor
 */
Target::Target()
{
  // init values
  az = el = range = rate = 0;
  memset(TLE_L0, 0, sizeof(TLE_L0));
  memset(TLE_L1, 0, sizeof(TLE_L1));
  memset(TLE_L2, 0, sizeof(TLE_L2));
  sat = new Satellite();
  sun = new Sun();

  // init flags
  tle_ok = false;
  tracking = false;
  overridden = false;
  set_ok = rise_ok = trans_ok = false;
  nskypath = 0;
}

/* update target if valid, and move rotator if tracking
 */
void Target::track()
{
  // update ephemerides
  updateTopo();

  // update rotator if tracking
  if(tracking)
  {
    rotator->moveToAzEl (az, el);
  }
}

/* update target info
 */
void Target::updateTopo()
{
  if(tle_ok && !overridden)
  {
    DateTime now(gps->now());
    sat->predict(now);
    sun->predict(now);
    sat->topo(gps->observer(), el, az, range, rate);
  }
}

/* turn tracking on or off if it makes sense to do so
 */
void Target::setTrackingState(bool want_on)
{
  if (want_on)
  {
//    if(!rotator->connected())
//    {
//      webpage->setUserMessage("Can not track without a rotator!");
//      tracking = false;
//    }
//    else if(!rotator->calibrated())
//    {
//      webpage->setUserMessage("Calibrating rotator motor scales!");
//      tracking = true;
//    }
//    else if(!IMU->connected())
    if(!imuA->connected())
    {
      webpage->setUserMessage("Can not track without a position sensor!");
      tracking = false;
    }
    else if(overridden)
    {
      webpage->setUserMessage("Now tracking overridden Az and El+");
      tracking = true;
    }
    else if(tle_ok)
    {
      webpage->setUserMessage("Now tracking: ", TLE_L0, '+');
      tracking = true;
    }
    else
    {
      webpage->setUserMessage("First Upload a TLE or override Target Az and El!");
      tracking = false;
    }
  }
  else
  {
    webpage->setUserMessage("Tracking is off");
    tracking = false;
  }
}

/* send latest values to web page.
 * N.B. names must match ids in web page
 */
void Target::sendNewValues (WiFiClient client)
{
  char zerostr[] = "";

  // update pass info just after main events
  // N.B. transit jiggles back and forth too much to recompute after it
  DateTime now(gps->now());
  bool just_rose = rise_ok && now.diff(rise_time) < 0;
  bool just_sat = set_ok && now.diff(set_time) < 0;
  if(just_rose || just_sat)
  {
    updateTopo();
    findNextPass();
    computeSkyPath();
  }

  if(tle_ok || overridden)
  {
    client.print("T_Az=");
    client.print(az);
    displayAsWarning(client, overridden);
    client.print("T_El=");
    client.print(el);
    displayAsWarning(client, overridden);
  }

  if(tle_ok && !overridden)
  {
    float age = gps->age(sat);
    client.print("T_Age=");
    client.print(age);
    displayAsWarning(client, age < -10 || age > 10);
    
    client.print("T_Sunlit=");
    if(sat->eclipsed(sun))
    {
      client.println("No");
    }
    else
    {
      client.println("Yes");
    }

    client.print("T_Range=");
    client.println(range);
    client.print("T_RangeR=");
    client.println(rate);
    client.print("T_VHFDoppler=");
    client.println(-rate * 144000 / 3e8); // want kHz
    client.print("T_UHFDoppler=");
    client.println(-rate * 440000 / 3e8); // want kHz

    client.print("T_NextRise=");
    if(rise_ok)
    {
      float dt = 24 * now.diff(rise_time);
      gps->printSexa(client, dt);
      gps->printPL(client, (dt < 1.0 / 60.0) ? GPS::GOODNEWS : GPS::NORMAL);
    }
    else
    {
      client.println("??? !"); // beware trigraphs
    }

    client.print("T_RiseAz=");
    if(rise_ok)
    {
      client.println(rise_az);
    }
    else
    {
      client.println("??? !");
    }

    char *transin, *transaz, *transel;
    client.print("T_NextTrans=");
    if(trans_ok)
    {
      float dt = 24 * now.diff(trans_time);
      gps->printSexa(client, dt);
      gps->printPL(client, GPS::NORMAL);
      if(dt < 0)
      {
        transin = "This Transited ago";
        transaz = "This Transit Azimuth";
        transel = "This Transit Elevation";
      }
      else
      {
        transin = "Next Transit in";
        transaz = "Next Transit Azimuth";
        transel = "Next Transit Elevation";
      }
    }
    else
    {
      client.println("??? !");
      transin = zerostr;
      transaz = zerostr;
      transel = zerostr;
    }
    client.print("T_NextTrans_l=");
    client.println(transin);
    
    client.print("T_TransAz=");
    if(trans_ok)
    {
      client.println(trans_az);
    }
    else
    {
      client.println("??? !");
      client.print("T_TransAz_l=");
      client.println(transaz);
    }
    client.print("T_TransEl=");
    if (trans_ok)
    {
      client.println (trans_el);
    }
    else
    {
      client.println (F("??? !"));
      client.print ("T_TransEl_l=");
      client.println (transel);
    }

    client.print("T_NextSet=");
    if(set_ok)
    {
      float dt = 24*now.diff(set_time);
      gps->printSexa(client, dt);
      gps->printPL(client, GPS::NORMAL);
    }
    else
    {
      client.println("??? !");
    }
    
    client.print("T_SetAz=");
    if(rise_ok)
    {
      client.println(set_az);
    }
    else
    {
      client.println("??? !");
    }

    char *tup;
    client.print("T_Up=");
    if(rise_ok && set_ok)
    {
      float up = rise_time.diff(set_time);
      if(up > 0)
      {
        gps->printSexa(client, up*24); // next whole pass
        gps->printPL(client, GPS::NORMAL);
        tup = "Next pass duration";
      }
      else
      {
        up = 24*now.diff(set_time); // this pass remaining
        gps->printSexa(client, up);
        gps->printPL(client, (up < 1.0/60.0) ? GPS::BADNEWS : GPS::NORMAL);
        tup = "This pass Ends in";
      }
    }
    else
    {
      tup = zerostr;
      client.println(zerostr);
    }
    client.print("T_Up_l=");
    client.println(tup);
  }

  client.print("T_TLE=");
  if(tle_ok)
  {
    client.println(TLE_L0);
    client.println(TLE_L1);
    client.println(TLE_L2);
  }
  else
  {
    client.println(zerostr);
    client.println(zerostr);
    client.println(zerostr);
  }

  if(overridden)
  {
    client.println("T_Age=");
    client.println("T_Sunlit=");
    client.println("T_Range=");
    client.println("T_RangeR=");
    client.println("T_VHFDoppler=");
    client.println("T_UHFDoppler=");
    client.println("T_NextRise=");
    client.println("T_RiseAz=");
    client.println("T_NextTrans=");
    client.println("T_TransAz=");
    client.println("T_TransEl=");
    client.println("T_NextSet=");
    client.println("T_SetAz=");
  }

  client.print("T_Status=");
  client.println(tle_ok || overridden ? (el > 0 ? "Up+" : "Down") : "No target!");

  client.print("tracking=");
  if(tracking)
  {
    client.println("On");
  }
  else
  {
    client.println("Off");
  }

  client.print("skypath=");
  for(uint8_t i = 0; i < nskypath; i++)
  {
    client.print(skypath[i].az);
    client.print(",");
    client.print(skypath[i].el);
    client.print(";");
  }
  client.println(zerostr);
}

/* optionally add code to display the value as a warning
 */
void Target::displayAsWarning(WiFiClient client, bool mark)
{
  if(mark)
  {
    client.println("!");
  }
  else
  {
    client.println("");
  }
}

/* process name = value other than T_TLE (that's done with setTLE()).
 * return whether we recognize it
 */
bool Target::overrideValue(char *name, char *value)
{
  if(!strcmp(name, "T_Az"))
  {
    az = atof(value);
    while(az < 0)
    {
      az += 360;
    }
    while(az >= 360)
    {
      az -= 360;
    }
    overridden = true;
    tle_ok = false;
    nskypath = 0;
    return true;
  }
  if(!strcmp (name, "T_El"))
  {
    el = fmax(fmin(atof(value), 90), 0);
    overridden = true;
    tle_ok = false;
    nskypath = 0;
    return true;
  }
  return false;
}

/* record a TLE for possible tracking, set tle_ok if valid
 */
void Target::setTLE(char *l1, char *l2, char *l3)
{
  if(tleValidChecksum (l2) && tleValidChecksum(l3))
  {
    tle_ok = true;
    sat->tle (l2, l3);
    strncpy(TLE_L0, l1, sizeof(TLE_L0)-1);
    strncpy(TLE_L1, l2, sizeof(TLE_L1)-1);
    strncpy(TLE_L2, l3, sizeof(TLE_L2)-1);
    Serial.println(TLE_L0);
    Serial.println(TLE_L1);
    Serial.println(TLE_L2);
    overridden = false;
    tracking = false;
    updateTopo();
    findNextPass(); // init for track()
    computeSkyPath();
    webpage->setUserMessage("New TLE uploaded successfully for ", TLE_L0, '+');
  }
  else
  {
    webpage->setUserMessage("Uploaded TLE is invalid!");
    tle_ok = false;
  }
}

/* find next pass for sat, if currently valid
 */
void Target::findNextPass()
{
  if(!tle_ok || overridden)
  {
    set_ok = rise_ok = trans_ok = false;
    return;
  }

  const int8_t COARSE_DT = 60; // seconds/step forward for course search
  const int8_t FINE_DT = -1; // seconds/step backward for refined search
  float pel = 0, ppel = 0; // previous 2 elevations
  float paz = 0; // previous az
  int8_t dt = COARSE_DT; // search step size, seconds
  DateTime t(gps->now()); // search time, init with now

  // search no more than two days ahead
  set_ok = rise_ok = trans_ok = false;
  while((!set_ok || !rise_ok || !trans_ok) && gps->now().diff(t) < 2)
  {
    // find circumstances at time t
    float tel, taz, trange, trate;
    sat->predict(t);
    sat->topo(gps->observer(), tel, taz, trange, trate);
    // Serial.print(24*60*gps->now().diff(t)); Serial.print(" ");
    // Serial.print(tel, 6); Serial.print(" ");
    // Serial.print(rise_ok); Serial.print (trans_ok); Serial.println (set_ok);
    
    // check for a visible transit event
    // N.B. too flat to use FINE_DT
    if(dt == COARSE_DT && tel > 0 && ppel > 0 && ppel < pel && pel > tel)
    {
      // found a coarse transit at previous time, good enough
      trans_time = t;
      trans_time.add ((long)(-COARSE_DT));
      trans_az = paz;
      trans_el = pel;
      trans_ok = true;
    }
    
    // check for rising events
    // N.B. invalidate ppel after turning around
    if(tel > 0 && pel < 0)
    {
      if(dt == FINE_DT)
      {
        // going backwards so found a refined set event, record and resume course forward time
        set_time = t;
        set_az = taz;
        set_ok = true;
        dt = COARSE_DT;
        pel = tel;
      }
      else if(!rise_ok)
      {
        // found a coarse rise event, go back slower looking for better set
        dt = FINE_DT;
        pel = tel;
      }
    }
    
    // check for setting events
    // N.B. invalidate ppel after turning around
    if(tel < 0 && pel > 0)
    {
      if(dt == FINE_DT)
      {
        // going backwards so found a refined rise event, record and resume course forward time
        rise_time = t;
        rise_az = taz;
        rise_ok = true;
        dt = COARSE_DT;
        pel = tel;
      }
      else if(!set_ok)
      {
        // found a coarse set event, go back slower looking for better rise
        dt = FINE_DT;
        pel = tel;
      }
    }
    
    // next time step
    paz = taz;
    ppel = pel;
    pel = tel;
    t.add((long)dt);
  }
}

/* compute sky path of current pass.
 * if up now just plot until set because rise_time will be for subsequent pass
 */
void Target::computeSkyPath()
{
  if(!set_ok || !rise_ok)
  {
    return;
  }
  
  DateTime t;
  
  if(el > -1)  // allow for a bit of rise/set round off
  {
    t = gps->now();
  }
  else if(rise_time.diff(set_time) > 0)
  {
    t = rise_time;
  }
  else
  {
    // rise or set is unknown or for different passes
    nskypath = 0;
    return;
  }
  
  long secsup = (long)(t.diff(set_time) * 24 * 3600);
  long stepsecs = secsup / (MAXSKYPATH - 1); // inclusive
  
  for(nskypath = 0; nskypath < MAXSKYPATH; nskypath++)
  {
    float srange, srate;
    sat->predict(t);
    sat->topo(gps->observer(), skypath[nskypath].el, skypath[nskypath].az, srange, srate);
    t.add(stepsecs);
  }
}

/* return whether the given line appears to be a valid TLE
 * only count digits and '-' counts as 1
 */
bool Target::tleValidChecksum(const char *line)
{
  // sum first 68 chars
  int sum = 0;
  for(uint8_t i = 0; i < 68; i++)
  {
    char c = *line++;
    if (c == '-')
    {
      sum += 1;
    }
    else if (c == '\0')
    {
      return (false); // too short
    }
    else if(c >= '0' && c <= '9')
    {
      sum += c - '0';
    }
  }

  // last char is sum of previous modulo 10
  return (*line - '0') == (sum%10);
}
