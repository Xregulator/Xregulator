// X Engineering Alternator Regulator
// Copyright (C) 2026 X Engineering LLC
// Contact: joe@xengineering.net

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.


void SystemTime(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  uint16_t SystemDate;
  double SystemTime;
  tN2kTimeSource TimeSource;

  if (ParseN2kSystemTime(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
    lastNmea2kSystemTimeMs = millis();  // freshness for priority chain (NMEA > Phone > NTP)
    // Sync time from GPS (Phase 2 Sensor History)
    syncTimeFromGPS(SystemDate, SystemTime);

    // Original verbose output (only if enabled)
    if (NMEA2KVerbose) {
      OutputStream->println("System time:");
      PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
      PrintLabelValWithConversionCheckUnDef("  days since 1.1.1970: ", SystemDate, 0, true);
      PrintLabelValWithConversionCheckUnDef("  seconds since midnight: ", SystemTime, 0, true);
      OutputStream->print("  time source: ");
      PrintN2kEnumType(TimeSource, OutputStream);
    }
  } else {
    if (NMEA2KVerbose) {
      OutputStream->print("Failed to parse PGN: ");
      OutputStream->println(N2kMsg.PGN);
    }
  }
}

void Rudder(const tN2kMsg &N2kMsg) {
  unsigned char Instance;
  tN2kRudderDirectionOrder RudderDirectionOrder;
  double RudderPosition;
  double AngleOrder;

  if (ParseN2kRudder(N2kMsg, RudderPosition, Instance, RudderDirectionOrder, AngleOrder)) {
    //Uncomment below to get serial montior back
    // PrintLabelValWithConversionCheckUnDef("Rudder: ", Instance, 0, true);
    // PrintLabelValWithConversionCheckUnDef("  position (deg): ", RudderPosition, &RadToDeg, true);
    // OutputStream->print("  direction order: ");
    // PrintN2kEnumType(RudderDirectionOrder, OutputStream);
    // PrintLabelValWithConversionCheckUnDef("  angle order (deg): ", AngleOrder, &RadToDeg, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void Heading(const tN2kMsg &N2kMsg) {
  static unsigned long lastHeadingUpdate = 0;
  if (millis() - lastHeadingUpdate < 2000) return;  // Throttle to once every 2 seconds
  lastHeadingUpdate = millis();
  unsigned char SID;
  tN2kHeadingReference HeadingReference;
  double Heading;
  double Deviation;
  double Variation;

  if (ParseN2kHeading(N2kMsg, SID, Heading, Deviation, Variation, HeadingReference)) {
    if (N2kIsNA(Heading)) return;        // F-RES-04: skip NA field, otherwise -1e9 leaks into HeadingNMEA
    // Parsing succeeded - update variable and mark fresh
    HeadingNMEA = Heading * 180.0 / PI;  // Convert radians to degrees
    MARK_FRESH(IDX_HEADING_NMEA);        // Only called on successful parse

    // Uncomment below to get serial monitor output back
    // OutputStream->println("Heading:");
    // PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    // OutputStream->print("  reference: ");
    // PrintN2kEnumType(HeadingReference, OutputStream);
    // PrintLabelValWithConversionCheckUnDef("  Heading (deg): ", Heading, &RadToDeg, true);
    // PrintLabelValWithConversionCheckUnDef("  Deviation (deg): ", Deviation, &RadToDeg, true);
    // PrintLabelValWithConversionCheckUnDef("  Variation (deg): ", Variation, &RadToDeg, true);
  } else {
    // Parsing failed - do NOT call MARK_FRESH, data will go stale automatically
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void COGSOG(const tN2kMsg &N2kMsg) {
  static unsigned long lastCOGSOGUpdate = 0;
  if (millis() - lastCOGSOGUpdate < 2000) return;
  lastCOGSOGUpdate = millis();

  unsigned char SID;
  tN2kHeadingReference HeadingReference;
  double COG;
  double SOG;

  if (ParseN2kCOGSOGRapid(N2kMsg, SID, HeadingReference, COG, SOG)) {
    // F-RES-04: bail entirely if both fields NA; otherwise update each independently.
    if (N2kIsNA(COG) && N2kIsNA(SOG)) return;
    if (!N2kIsNA(COG)) {
      COGNMEA = COG * 180.0 / PI;  // radians → degrees (matches Heading() pattern)
      MARK_FRESH(IDX_COG_NMEA);
    }
    if (!N2kIsNA(SOG)) {
      SOGNMEA = SOG * 1.94384;     // m/s → knots (matches WindSpeed() pattern)
      MARK_FRESH(IDX_SOG_NMEA);

      if (SOGNMEA > MaxSpeed) {
        MaxSpeed = SOGNMEA;
      }
      if (SOGNMEA > MaxSpeed_AllTime) {
        MaxSpeed_AllTime = SOGNMEA;
      }
      wmIgnUpdate(wmIgn_SOG, SOGNMEA);  // ignition-cycle watermark
    }

  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}

//*****************************************************************************

void GNSS(const tN2kMsg &N2kMsg) {
  //Serial.println("=== GNSS function called ===");
  //Serial.printf("PGN: %lu\n", N2kMsg.PGN);
  static unsigned long lastGNSSUpdate = 0;
  if (millis() - lastGNSSUpdate < 2000) return;  // Throttle to once every 2 seconds
  lastGNSSUpdate = millis();

  unsigned char SID;
  uint16_t DaysSince1970;
  double SecondsSinceMidnight;
  double Latitude;
  double Longitude;
  double Altitude;
  tN2kGNSStype GNSStype;
  tN2kGNSSmethod GNSSmethod;
  unsigned char nSatellites;
  double HDOP;
  double PDOP;
  double GeoidalSeparation;
  unsigned char nReferenceStations;
  tN2kGNSStype ReferenceStationType;
  uint16_t ReferenceStationID;
  double AgeOfCorrection;

  if (ParseN2kGNSS(N2kMsg, SID, DaysSince1970, SecondsSinceMidnight,
                   Latitude, Longitude, Altitude,
                   GNSStype, GNSSmethod,
                   nSatellites, HDOP, PDOP, GeoidalSeparation,
                   nReferenceStations, ReferenceStationType, ReferenceStationID,
                   AgeOfCorrection)) {

    //Serial.println("GNSS parsing SUCCESS!");
    //Serial.printf("Raw values - Lat: %f, Lon: %f, Sats: %d\n", Latitude, Longitude, nSatellites);

    // Check if we have valid GPS data (not NaN and reasonable values)
    if (!isnan(Latitude) && !isnan(Longitude) && Latitude != 0.0 && Longitude != 0.0 && abs(Latitude) <= 90.0 && abs(Longitude) <= 180.0 && nSatellites > 0) {

      //Store values globally for web interface
      LatitudeNMEA = Latitude;
      LongitudeNMEA = Longitude;
      SatelliteCountNMEA = nSatellites;
      lastNmea2kGnssMs = millis();  // freshness for GPS priority chain (NMEA > Phone)
      currentGpsSource = GPS_NMEA;  // NMEA wins when present; consumePhoneGps() flips this when NMEA goes stale

      // Mark all GPS data as fresh - only called on valid data
      MARK_FRESH(IDX_LATITUDE_NMEA);
      MARK_FRESH(IDX_LONGITUDE_NMEA);
      MARK_FRESH(IDX_SATELLITE_COUNT);

      //Serial.printf("Valid GNSS data stored - LatNMEA: %f, LonNMEA: %f, SatNMEA: %d\n", LatitudeNMEA, LongitudeNMEA, SatelliteCountNMEA);
    } else {
      //Serial.println("GNSS data invalid - values are NaN, zero, or out of range");
      // Invalid data - do NOT call MARK_FRESH, let data go stale
    }
  } else {
    //Serial.println("GNSS parsing FAILED!");
    // Parse failed - do NOT call MARK_FRESH, let data go stale
  }
}
//*****************************************************************************
void GNSSSatsInView(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  tN2kRangeResidualMode Mode;
  uint8_t NumberOfSVs;
  tSatelliteInfo SatelliteInfo;

  if (ParseN2kPGNSatellitesInView(N2kMsg, SID, Mode, NumberOfSVs)) {

    if (NMEA2KVerbose) {
      OutputStream->println("Satellites in view: ");
      OutputStream->print("  mode: ");
      OutputStream->println(Mode);
      OutputStream->print("  number of satellites: ");
      OutputStream->println(NumberOfSVs);
      for (uint8_t i = 0; i < NumberOfSVs && ParseN2kPGNSatellitesInView(N2kMsg, i, SatelliteInfo); i++) {
        OutputStream->print("  Satellite PRN: ");
        OutputStream->println(SatelliteInfo.PRN);
        PrintLabelValWithConversionCheckUnDef("    elevation: ", SatelliteInfo.Elevation, &RadToDeg, true, 1);
        PrintLabelValWithConversionCheckUnDef("    azimuth:   ", SatelliteInfo.Azimuth, &RadToDeg, true, 1);
        PrintLabelValWithConversionCheckUnDef("    SNR:       ", SatelliteInfo.SNR, 0, true, 1);
        PrintLabelValWithConversionCheckUnDef("    residuals: ", SatelliteInfo.RangeResiduals, 0, true, 1);
        OutputStream->print("    status: ");
        OutputStream->println(SatelliteInfo.UsageStatus);
      }
    }
  }
}
//*****************************************************************************
void BatteryConfigurationStatus(const tN2kMsg &N2kMsg) {
  unsigned char BatInstance;
  tN2kBatType BatType;
  tN2kBatEqSupport SupportsEqual;
  tN2kBatNomVolt BatNominalVoltage;
  tN2kBatChem BatChemistry;
  double BatCapacity;
  int8_t BatTemperatureCoefficient;
  double PeukertExponent;
  int8_t ChargeEfficiencyFactor;

  if (ParseN2kBatConf(N2kMsg, BatInstance, BatType, SupportsEqual, BatNominalVoltage, BatChemistry, BatCapacity, BatTemperatureCoefficient, PeukertExponent, ChargeEfficiencyFactor)) {
    PrintLabelValWithConversionCheckUnDef("Battery instance: ", BatInstance, 0, true);
    OutputStream->print("  - type: ");
    PrintN2kEnumType(BatType, OutputStream);
    OutputStream->print("  - support equal.: ");
    PrintN2kEnumType(SupportsEqual, OutputStream);
    OutputStream->print("  - nominal voltage: ");
    PrintN2kEnumType(BatNominalVoltage, OutputStream);
    OutputStream->print("  - chemistry: ");
    PrintN2kEnumType(BatChemistry, OutputStream);
    PrintLabelValWithConversionCheckUnDef("  - capacity (Ah): ", BatCapacity, &CoulombToAh, true);
    PrintLabelValWithConversionCheckUnDef("  - temperature coefficient (%): ", BatTemperatureCoefficient, 0, true);
    PrintLabelValWithConversionCheckUnDef("  - peukert exponent: ", PeukertExponent, 0, true);
    PrintLabelValWithConversionCheckUnDef("  - charge efficiency factor (%): ", ChargeEfficiencyFactor, 0, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void DCStatus(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  unsigned char DCInstance;
  tN2kDCType DCType;
  unsigned char StateOfCharge;
  unsigned char StateOfHealth;
  double TimeRemaining;
  double RippleVoltage;
  double Capacity;

  if (ParseN2kDCStatus(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity)) {
    OutputStream->print("DC instance: ");
    OutputStream->println(DCInstance);
    OutputStream->print("  - type: ");
    PrintN2kEnumType(DCType, OutputStream);
    OutputStream->print("  - state of charge (%): ");
    OutputStream->println(StateOfCharge);
    OutputStream->print("  - state of health (%): ");
    OutputStream->println(StateOfHealth);
    OutputStream->print("  - time remaining (h): ");
    OutputStream->println(TimeRemaining / 60);
    OutputStream->print("  - ripple voltage: ");
    OutputStream->println(RippleVoltage);
    OutputStream->print("  - capacity: ");
    OutputStream->println(Capacity);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void Speed(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double SOW;
  double SOG;
  tN2kSpeedWaterReferenceType SWRT;

  if (ParseN2kBoatSpeed(N2kMsg, SID, SOW, SOG, SWRT)) {
    // Surface speed-through-water (SOW) for the boat-performance polar — removes current,
    // unlike SOG. SOG stays sourced from the COG/SOG rapid PGN (its canonical source).
    if (!N2kIsNA(SOW)) {
      STWNMEA = SOW * 1.94384;      // m/s → knots
      MARK_FRESH(IDX_STW_NMEA);
    }
    OutputStream->print("Boat speed:");
    PrintLabelValWithConversionCheckUnDef(" SOW:", N2kIsNA(SOW) ? SOW : msToKnots(SOW));
    PrintLabelValWithConversionCheckUnDef(", SOG:", N2kIsNA(SOG) ? SOG : msToKnots(SOG));
    OutputStream->print(", ");
    PrintN2kEnumType(SWRT, OutputStream, true);
  }
}
void WindSpeed(const tN2kMsg &N2kMsg) {
  static unsigned long lastWindUpdate = 0;
  if (millis() - lastWindUpdate < 2000) return;
  lastWindUpdate = millis();

  unsigned char SID;
  double WindSpeed;
  double WindAngle;
  tN2kWindReference WindReference;

  if (ParseN2kWindSpeed(N2kMsg, SID, WindSpeed, WindAngle, WindReference)) {
    // F-RES-04: bail entirely if both fields NA; otherwise update each independently.
    if (N2kIsNA(WindSpeed) && N2kIsNA(WindAngle)) return;

    // PGN 130306 carries either apparent OR true wind — branch on WindReference so we
    // don't dump true-wind values into apparent globals (the bug we hit when a network
    // published true-N directly; calculateDerivedMetrics then ran apparent→true math on
    // them again, producing nonsense). True_North and Magnetic are earth-frame TWD; we
    // convert to boat-relative TWA via HeadingNMEA so TrueWindAngleNMEA stays consistent
    // with the existing apparent→true derivation. True_boat / True_water are already
    // boat-relative. Apparent path unchanged.
    bool isTrue = (WindReference == N2kWind_True_North ||
                   WindReference == N2kWind_Magnetic ||
                   WindReference == N2kWind_True_boat ||
                   WindReference == N2kWind_True_water);

    if (isTrue) {
      if (!N2kIsNA(WindSpeed)) {
        TrueWindSpeedNMEA = WindSpeed * 1.94384;
        MARK_FRESH(IDX_TRUE_WIND_SPEED);
      }
      if (!N2kIsNA(WindAngle)) {
        float angDeg = WindAngle * 180.0 / PI;
        while (angDeg < 0)       angDeg += 360.0;
        while (angDeg >= 360.0)  angDeg -= 360.0;
        if ((WindReference == N2kWind_True_North || WindReference == N2kWind_Magnetic) &&
            !IS_STALE(IDX_HEADING_NMEA)) {
          // Earth-frame TWD → convert to boat-relative TWA
          float twa = angDeg - HeadingNMEA;
          while (twa < 0)        twa += 360.0;
          while (twa >= 360.0)   twa -= 360.0;
          TrueWindAngleNMEA = twa;
        } else {
          // True_boat / True_water → already boat-relative
          // (or earth-frame without heading — fall through; better than nothing for Zambretti
          //  which adds heading back in JS)
          TrueWindAngleNMEA = angDeg;
        }
        MARK_FRESH(IDX_TRUE_WIND_ANGLE);
      }
      if (!N2kIsNA(WindSpeed)) UpdateWindMaximums();
    } else {
      // Apparent (N2kWind_Apparent or unknown reference)
      if (!N2kIsNA(WindSpeed)) {
        ApparentWindSpeedNMEA = WindSpeed * 1.94384;
        MARK_FRESH(IDX_APPARENT_WIND_SPEED);
      }
      if (!N2kIsNA(WindAngle)) {
        ApparentWindAngleNMEA = WindAngle * 180.0 / PI;
        MARK_FRESH(IDX_APPARENT_WIND_ANGLE);
      }
      if (!N2kIsNA(WindSpeed)) UpdateWindMaximums();  // also guards TrueWindSpeedNMEA internally
    }

  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void WaterDepth(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double DepthBelowTransducer;
  double Offset;

  if (ParseN2kWaterDepth(N2kMsg, SID, DepthBelowTransducer, Offset)) {
    // Store effective depth (transducer + offset if available; raw transducer otherwise).
    // Offset > 0 = depth below waterline; Offset < 0 = depth below keel. Either way the sum is the
    // physical water depth at the transducer position.
    if (!N2kIsNA(DepthBelowTransducer)) {
      double effectiveDepth_m = DepthBelowTransducer + (N2kIsNA(Offset) ? 0.0 : Offset);
      if (effectiveDepth_m >= 0) {
        WaterDepth_m = (float)effectiveDepth_m;
        MARK_FRESH(IDX_WATER_DEPTH);
      }
    }
    if (N2kIsNA(Offset) || Offset == 0) {
      PrintLabelValWithConversionCheckUnDef("Depth below transducer", DepthBelowTransducer);
      if (N2kIsNA(Offset)) {
        OutputStream->println(", offset not available");
      } else {
        OutputStream->println(", offset=0");
      }
    } else {
      if (Offset > 0) {
        OutputStream->print("Water depth:");
      } else {
        OutputStream->print("Depth below keel:");
      }
      if (!N2kIsNA(DepthBelowTransducer)) {
        OutputStream->println(DepthBelowTransducer + Offset);
      } else {
        OutputStream->println(" not available");
      }
    }
  }
}
//*****************************************************************************
void Attitude(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double Yaw;
  double Pitch;
  double Roll;

  if (ParseN2kAttitude(N2kMsg, SID, Yaw, Pitch, Roll)) {
    OutputStream->println("Attitude:");
    PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    PrintLabelValWithConversionCheckUnDef("  Yaw (deg): ", Yaw, &RadToDeg, true);
    PrintLabelValWithConversionCheckUnDef("  Pitch (deg): ", Pitch, &RadToDeg, true);
    PrintLabelValWithConversionCheckUnDef("  Roll (deg): ", Roll, &RadToDeg, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
  int iHandler;
  // Find handler
  if (NMEA2KVerbose == 1) {
    OutputStream->print("In Main Handler: ");
    OutputStream->println(N2kMsg.PGN);
  }
  for (iHandler = 0; NMEA2000Handlers[iHandler].PGN != 0 && !(N2kMsg.PGN == NMEA2000Handlers[iHandler].PGN); iHandler++)
    ;

  if (NMEA2000Handlers[iHandler].PGN != 0) {
    NMEA2000Handlers[iHandler].Handler(N2kMsg);
  }
}

void ReadVEData() {
  if (VeData != 1) {
    return;
  }
  static unsigned long lastVEDataRead = 0;
  static unsigned long lastSolarEnergyUpdate = 0;
  const unsigned long VE_DATA_INTERVAL = 2000;  // 2 seconds

  unsigned long currentTime = millis();

  // Check if it's time to read VE data
  if (currentTime - lastVEDataRead <= VE_DATA_INTERVAL) {
    return;
  }

  int start1 = micros();      // Start timing VeData
  bool dataReceived = false;  // Track if we got any valid data
  float solarPower_W = 0.0f;  // Track solar power for this update

  int veDrainBudget = 256;  // cap per-tick drain so a chatty Victron can't starve the loop
  while (veDrainBudget-- > 0 && Serial1.available()) {
    myve.rxData(Serial1.read());
    for (int i = 0; i < myve.veEnd; i++) {
      if (strcmp(myve.veName[i], "V") == 0) {
        float newVoltage = (atof(myve.veValue[i]) / 1000);
        if (newVoltage > 0 && newVoltage < 100) {  // Sanity check
          VictronVoltage = newVoltage;
          MARK_FRESH(IDX_VICTRON_VOLTAGE);  // Only mark fresh on valid data
          dataReceived = true;
        }
      }
      if (strcmp(myve.veName[i], "I") == 0) {
        float newCurrent = (atof(myve.veValue[i]) / 1000);
        if (newCurrent > -1000 && newCurrent < 1000) {  // Sanity check
          VictronCurrent = newCurrent;
          MARK_FRESH(IDX_VICTRON_CURRENT);  // Only mark fresh on valid data
          dataReceived = true;
        }
      }
      if (strcmp(myve.veName[i], "PPV") == 0) {
        solarPower_W = atof(myve.veValue[i]);             // PPV is already in Watts
        if (solarPower_W >= 0 && solarPower_W < 10000) {  // Sanity check (0-10kW)
          dataReceived = true;
          VictronSolarPower_W = solarPower_W;             // expose live for dashboard + leaderboard
          MARK_FRESH(IDX_VICTRON_SOLAR);
          if (solarPower_W > solar_power_max_alltime_w) solar_power_max_alltime_w = solarPower_W;  // leaderboard: peak solar power
        } else {
          solarPower_W = 0.0f;  // Invalid reading, set to zero
        }
      }
      if (strcmp(myve.veName[i], "VPV") == 0) {
        float vpv = atof(myve.veValue[i]) / 1000.0f;      // VPV is in mV -> V
        if (vpv >= 0 && vpv < 400) {                      // Sanity check (0-400V panel array)
          VictronSolarVoltage_V = vpv;
          dataReceived = true;
        }
      }
      // MPPT status + daily-yield fields (all free from the same VE.Direct frame)
      if (strcmp(myve.veName[i], "CS") == 0)   { VictronChargeState = atoi(myve.veValue[i]); dataReceived = true; }
      if (strcmp(myve.veName[i], "MPPT") == 0) { VictronMPPTMode    = atoi(myve.veValue[i]); dataReceived = true; }
      if (strcmp(myve.veName[i], "ERR") == 0)  { VictronError       = atoi(myve.veValue[i]); dataReceived = true; }
      if (strcmp(myve.veName[i], "H20") == 0)  { VictronYieldToday_kWh     = atof(myve.veValue[i]) / 100.0f; dataReceived = true; }  // 0.01 kWh units
      if (strcmp(myve.veName[i], "H21") == 0)  { VictronMaxPowerToday_W    = atof(myve.veValue[i]);          dataReceived = true; }
      if (strcmp(myve.veName[i], "H22") == 0)  { VictronYieldYesterday_kWh = atof(myve.veValue[i]) / 100.0f; dataReceived = true; }  // 0.01 kWh units
      if (strcmp(myve.veName[i], "H23") == 0)  { VictronMaxPowerYesterday_W = atof(myve.veValue[i]);         dataReceived = true; }
    }
    yield();  // Allow other processes to run
  }

  // Panel current derived from power / voltage (VE.Direct MPPTs report PPV + VPV, not panel A)
  VictronSolarCurrent_A = (VictronSolarVoltage_V > 1.0f) ? (VictronSolarPower_W / VictronSolarVoltage_V) : 0.0f;

  // Calculate solar energy if we got valid power data
  if (dataReceived && lastSolarEnergyUpdate > 0) {
    unsigned long elapsedMillis = currentTime - lastSolarEnergyUpdate;
    float elapsedSeconds = elapsedMillis / 1000.0f;
    float solarEnergyDelta_Wh = (solarPower_W * elapsedSeconds) / 3600.0f;

    // Accumulate solar energy with floating point precision
    static float solarEnergyAccumulator = 0.0f;
    static float solarEnergyAccumulator_AllTime = 0.0f;

    solarEnergyAccumulator += solarEnergyDelta_Wh;
    solarEnergyAccumulator_AllTime += solarEnergyDelta_Wh;

    if (solarEnergyAccumulator >= 1.0f) {
      SolarChargedEnergy += (int)solarEnergyAccumulator;
      solarEnergyAccumulator -= (int)solarEnergyAccumulator;
    }

    if (solarEnergyAccumulator_AllTime >= 1.0f) {
      SolarChargedEnergy_AllTime += (int)solarEnergyAccumulator_AllTime;
      solarEnergyAccumulator_AllTime -= (int)solarEnergyAccumulator_AllTime;
    }
  }

  lastSolarEnergyUpdate = currentTime;

  int end1 = micros();     // End timing
  VeTime = end1 - start1;  // Store elapsed time
  lastVEDataRead = currentTime;
}
void checkAndRestart() {
  unsigned long currentMillis = millis();

  // Handle millis wraparound
  if (currentMillis < lastRestartTime) {
    lastRestartTime = 0;
  }

  unsigned long elapsedMs = currentMillis - lastRestartTime;

  // Warning window: publish remaining seconds for the dashboard banner + popup.
  // 0 = outside window. Floors at 1 until the actual reboot fires.
  if (elapsedMs + RESTART_WARNING_WINDOW_MS >= RESTART_INTERVAL && elapsedMs < RESTART_INTERVAL) {
    unsigned long remainingMs = RESTART_INTERVAL - elapsedMs;
    uint32_t sec = (uint32_t)(remainingMs / 1000UL);
    restartRemainingSec = (sec == 0) ? 1 : sec;
  } else if (elapsedMs < RESTART_INTERVAL) {
    restartRemainingSec = 0;
  }

  if (elapsedMs >= RESTART_INTERVAL) {
    restartRemainingSec = 1;  // keep banner visible through the shutdown sequence
    Serial.println("=== SCHEDULED RESTART APPROACHING ===");

    // Graceful field-down: zero PWM immediately and gate the PID via OnOff=0.
    // OnOff is RAM-only here — LittleFS is not touched, so on reboot the device
    // returns to whatever OnOff state the user last set.
    Serial.println("Restart: hard-zero PWM and disable charging");
    apply_pwm_float(0.0f);
    dutyCycle = 0.0f;
    lastAppliedDuty = 0.0f;
    OnOff = 0;
    delay(500);            // brief settle so any in-flight tick sees OnOff=0
    esp_task_wdt_reset();

    // NVS drain: persist counters/extrema/lifetime values before reboot.
    Serial.println("Restart: flushing NVS");
    saveNVSDataFull();
    esp_task_wdt_reset();

    // Wait for any in-progress uploads (max 30 seconds)
    unsigned long shutdownStart = millis();
    int waitCycles = 0;

    while (core0Busy && (millis() - shutdownStart < 30000)) {
      esp_task_wdt_reset();
      delay(500);
      waitCycles++;

      if (waitCycles % 10 == 0) {
        Serial.printf("Waiting for upload to complete... %ds\n",
                      (waitCycles * 500) / 1000);
      }
    }

    // Force-finish if still in progress
    if (core0Busy) {
      Serial.println("Upload still in progress after 30s, proceeding with restart");
      core0Busy = false;
    }

    // Wait for HTTP operations to fully complete
    delay(1000);
    esp_task_wdt_reset();

    // Save final sensor window if it has data — push into PSRAM ring so the
    // shutdown Phase 4 dump (or next-tick cloud upload) preserves it.
    if (currentWindow->battVolt_valid_us > 0) {
      Serial.println("Pushing final sensor window into ring before restart");
      time_t collectionTime = computeCollectionTime();
      pushSensorSnapshot(collectionTime);
      resetSensorWindow();
    }

    // Persist the long-term plot ring so the 12 h restart doesn't shed up to 15 min of
    // unflushed records (the periodic flush only fires every LONGTERM_DUMP_INTERVAL_MS).
    dumpLongTermRing();

    // Cleanly notify and close WiFi connections
    if (WiFi.getMode() != WIFI_OFF) {
      events.send("Performing scheduled restart", "console");
      events.send("Device restarting. Will reconnect shortly.", "status");
      delay(500);             // Give events time to actually send
      events.close();         // Close all SSE connections
      delay(100);             // Let close complete
      WiFi.disconnect(true);  // NOW disconnect WiFi
      delay(100);
    }

    Serial.println("=== RESTARTING NOW ===");
    Serial.flush();  // Ensure all Serial data is sent
    delay(100);      // Short delay for cleanup
    writeFile(LittleFS, "/ScheduledRestart.flag", "1");
    esp_task_wdt_delete(NULL);  // Disable watchdog before restart
    delay(100);                 // Short delay for cleanup
    ESP.restart();
  }
}
void captureResetReason() {
  String fileContent = settingRead(NK_LastResetReason);

  // Parse "last,ancient" format
  int commaPos = fileContent.indexOf(',');
  int previousLast = 0;
  int previousAncient = 0;

  if (commaPos > 0) {
    previousLast = fileContent.substring(0, commaPos).toInt();
    previousAncient = fileContent.substring(commaPos + 1).toInt();
  } else {
    previousLast = fileContent.toInt();  // Legacy: single value
    previousAncient = 0;
  }

  // Shift: previous last becomes new ancient
  ancientResetReason = previousLast;

  // Check if this was a scheduled restart
  bool wasScheduled = false;
  if (fsExists("/ScheduledRestart.flag")) {
    String flagContent = readFile(LittleFS, "/ScheduledRestart.flag");
    if (flagContent == "1") {
      wasScheduled = true;
    }
    fsRemove("/ScheduledRestart.flag");
  }

  // Get current reason
  int rawReason = (int)esp_reset_reason();
  switch (rawReason) {
    case ESP_RST_POWERON: LastResetReason = 0; break;
    case ESP_RST_SW:
      LastResetReason = wasScheduled ? 11 : 1;  // 11=scheduled, 1=unscheduled
      break;
    case ESP_RST_DEEPSLEEP: LastResetReason = 2; break;
    case ESP_RST_EXT: LastResetReason = 3; break;
    case ESP_RST_TASK_WDT: LastResetReason = 4; break;
    case ESP_RST_PANIC: LastResetReason = 5; break;
    case ESP_RST_BROWNOUT: LastResetReason = 6; break;
    case ESP_RST_INT_WDT: LastResetReason = 7; break;
    case ESP_RST_WDT: LastResetReason = 9; break;
    case ESP_RST_SDIO: LastResetReason = 10; break;
    default: LastResetReason = 8; break;
  }

  // Save both values: "current,ancient"
  String saveStr = String(LastResetReason) + "," + String(ancientResetReason);
  settingWrite(NK_LastResetReason, saveStr.c_str());
}
void UpdateEngineFuel(unsigned long elapsedMillis) {
  // Only calculate fuel consumption if engine is running. (High-RPM glitch reject is below, scaled
  // to the configured engine — a fixed 6000 ceiling would wrongly cut off high-revving engines.)
  if (RPM <= 0) {
    currentFuelGPH = 0.0f;  // engine off -> no live flow / economy
    currentNMPG = 0.0f;
    fcRun = false;          // break the steady-state run for the session fuel curve
    return;
  }

  float elapsedHours = elapsedMillis / 3600000.0f;  // Convert ms to hours

  // Find the number of valid entries (allow first entry to be zero for idle)
  int validEntries = 0;
  for (int i = 0; i < FUEL_TABLE_SIZE; i++) {
    if (fuelTableRPM[i] > 0 || i == 0) {  // Allow zero in first row
      validEntries = i + 1;
    } else {
      break;  // Stop at first zero (after row 0)
    }
  }

  // If no valid data, return
  if (validEntries == 0) {
    currentFuelGPH = 0.0f;  // no table configured -> no live flow / economy
    currentNMPG = 0.0f;
    fcRun = false;
    return;
  }

  // Engine RPM range = the configured fuel table. Top breakpoint defines the curve's bin scale and
  // a glitch ceiling that scales with the engine (no fixed 6000 cap, so high-rev engines aren't cut off).
  float fuelTopRPM = fuelTableRPM[validEntries - 1];
  if (fuelTopRPM < 1.0f) fuelTopRPM = 4500.0f;  // guard a degenerate table
  currentFuelTopRPM = fuelTopRPM;               // published for the chart x-axis
  if (RPM >= fuelTopRPM * 1.5f) {               // 50% over redline -> treat as sensor glitch
    currentFuelGPH = 0.0f;
    currentNMPG = 0.0f;
    fcRun = false;
    return;
  }

  // Interpolate fuel consumption based on current RPM
  float fuelRate_GPH = 0.0f;

  if (RPM <= fuelTableRPM[0]) {
    // Below first entry, use first GPH value
    fuelRate_GPH = fuelTableGPH[0];
  } else if (RPM >= fuelTableRPM[validEntries - 1]) {
    // Above last valid entry, use last valid GPH value
    fuelRate_GPH = fuelTableGPH[validEntries - 1];
  } else {
    // Find the two points to interpolate between
    for (int i = 0; i < validEntries - 1; i++) {
      if (RPM >= fuelTableRPM[i] && RPM < fuelTableRPM[i + 1]) {
        // Linear interpolation
        float rpmRange = fuelTableRPM[i + 1] - fuelTableRPM[i];
        float fuelRange = fuelTableGPH[i + 1] - fuelTableGPH[i];
        float rpmOffset = RPM - fuelTableRPM[i];
        fuelRate_GPH = fuelTableGPH[i] + (rpmOffset / rpmRange) * fuelRange;
        break;
      }
    }
  }

  // Publish live fuel flow + economy for the dashboard (gal/hr, naut mi/gal).
  // Economy = SOG (knots = naut mi/hr) / flow (gal/hr); 0 when not moving or no GPS speed.
  currentFuelGPH = fuelRate_GPH;
  currentNMPG = (fuelRate_GPH > 0.0f && SOGNMEA > 0.0f) ? (SOGNMEA / fuelRate_GPH) : 0.0f;

  // Session fuel-economy curve: settle-then-measure. RPM and boat speed must hold within band
  // (max-min ≤ tol on each) continuously; once they have held for fuelCurveSettleSec (the boat has
  // reached true steady speed for that throttle), mpg is averaged over the next fuelCurveSampleSec and
  // that average freezes the bin (overwriting). Then the settle->sample cycle restarts while still
  // steady, so a long cruise refreshes the bin. ANY band break / stop / no-GPS reseeds from scratch.
  if (currentNMPG > 0.0f && SOGNMEA > 0.0f) {
    uint32_t nowMs = millis();
    float r = RPM, s = SOGNMEA;
    bool inBand = false;
    if (fcRun) {
      float rMin = min(fcRpmMin, r), rMax = max(fcRpmMax, r);
      float sMin = min(fcSpdMin, s), sMax = max(fcSpdMax, s);
      if ((rMax - rMin) <= fuelCurveRpmTol && (sMax - sMin) <= fuelCurveSpdTol) {
        inBand = true;
        fcRpmMin = rMin; fcRpmMax = rMax; fcSpdMin = sMin; fcSpdMax = sMax;  // extend the run
        uint32_t elapsed = nowMs - fcRunStartMs;
        uint32_t settleMs = (uint32_t)(fuelCurveSettleSec * 1000.0f);
        uint32_t sampleMs = (uint32_t)(fuelCurveSampleSec * 1000.0f);
        if (elapsed >= settleMs) {                  // settled -> accumulate the sample window
          fcSampleMpgSum += currentNMPG;
          fcSampleRpmSum += r;
          fcSampleCount++;
        }
        if (elapsed >= settleMs + sampleMs) {       // sample window complete -> freeze the bin
          if (fcSampleCount > 0) {
            float avgMpg = (float)(fcSampleMpgSum / (double)fcSampleCount);
            float avgRpm = (float)(fcSampleRpmSum / (double)fcSampleCount);  // band centroid picks the bin
            float binW = fuelTopRPM / (float)FUELCURVE_BINS;                 // universal: width scales with engine
            int bin = (binW > 0.0f) ? (int)(avgRpm / binW) : -1;
            if (bin >= FUELCURVE_BINS) bin = FUELCURVE_BINS - 1;             // clamp top bin (RPM at/above redline)
            if (bin >= 0 && bin < FUELCURVE_BINS) fuelCurveNMPG[bin] = avgMpg;  // overwrite bin
          }
          // restart the settle->sample cycle, still steady (reseed band + accumulators at this sample)
          fcRunStartMs = nowMs;
          fcRpmMin = fcRpmMax = r; fcSpdMin = fcSpdMax = s;
          fcSampleMpgSum = 0; fcSampleRpmSum = 0; fcSampleCount = 0;
        }
      }
    }
    if (!inBand) {   // first entry, or band broke -> start a fresh settle, reseeded at this sample
      fcRun = true;
      fcRpmMin = fcRpmMax = r; fcSpdMin = fcSpdMax = s; fcRunStartMs = nowMs;
      fcSampleMpgSum = 0; fcSampleRpmSum = 0; fcSampleCount = 0;
    }
  } else {
    fcRun = false;  // stopped or no GPS speed -> break the run
  }

  // Calculate fuel consumed in this interval
  float fuelConsumed_Gallons = fuelRate_GPH * elapsedHours;
  float fuelConsumed_Liters = fuelConsumed_Gallons * 3.78541;  // Convert gallons to liters

  // Accumulate with floating point precision
  static float engineFuelAccumulator = 0.0f;
  static float engineFuelAccumulator_AllTime = 0.0f;

  engineFuelAccumulator += fuelConsumed_Liters;
  engineFuelAccumulator_AllTime += fuelConsumed_Liters;

  if (engineFuelAccumulator >= 0.01f) {
    float toAdd = floor(engineFuelAccumulator * 100.0f) / 100.0f;  // Round down to nearest 0.01 L
    EngineFuelUsed += toAdd;
    engineFuelAccumulator -= toAdd;  // Keep the fractional remainder
  }

  if (engineFuelAccumulator_AllTime >= 0.01f) {
    float toAdd = floor(engineFuelAccumulator_AllTime * 100.0f) / 100.0f;
    EngineFuelUsed_AllTime += toAdd;
    engineFuelAccumulator_AllTime -= toAdd;  // Keep the fractional remainder
  }
}
void UpdateBatterySOC(unsigned long elapsedMillis) {
  // Convert elapsed milliseconds to seconds for calculations
  float elapsedSeconds = elapsedMillis / 1000.0f;
  // =================================================================
  //     BATTERY STATE MONITORING
  // =================================================================
  // This section tracks battery state of charge from ALL sources:
  // Get current battery voltage and current readings
  float currentBatteryVoltage = getBatteryVoltage();
  Voltage_scaled = currentBatteryVoltage * 100;  // V × 100 for precision
  // Get battery current for SoC tracking (uses dedicated battery source)
  float batteryCurrentForSoC = getBatteryCurrent();
  BatteryCurrent_scaled = batteryCurrentForSoC * 100;  // A × 100 for precision
  // Calculate battery power (positive = charging, negative = discharging)
  BatteryPower_scaled = (Voltage_scaled * BatteryCurrent_scaled) / 100;  // W × 100
  // Energy calculations - convert power to energy over time
  float batteryPower_W = BatteryPower_scaled / 100.0f;
  float energyDelta_Wh = (batteryPower_W * elapsedSeconds) / 3600.0f;  // Power × time = energy
  // Energy accumulation with floating point precision
  static float chargedEnergyAccumulator = 0.0f;
  static float dischargedEnergyAccumulator = 0.0f;
  static float chargedEnergyAccumulator_AllTime = 0.0f;
  static float dischargedEnergyAccumulator_AllTime = 0.0f;
  if (BatteryCurrent_scaled > 0) {
    // Charging - energy going INTO battery
    chargedEnergyAccumulator += energyDelta_Wh;
    chargedEnergyAccumulator_AllTime += energyDelta_Wh;
    if (chargedEnergyAccumulator >= 1.0f) {
      ChargedEnergy += (int)chargedEnergyAccumulator;
      chargedEnergyAccumulator -= (int)chargedEnergyAccumulator;
    }
    if (chargedEnergyAccumulator_AllTime >= 1.0f) {
      ChargedEnergy_AllTime += (int)chargedEnergyAccumulator_AllTime;
      chargedEnergyAccumulator_AllTime -= (int)chargedEnergyAccumulator_AllTime;
    }
  } else if (BatteryCurrent_scaled < 0) {
    // Discharging - energy coming OUT of battery
    dischargedEnergyAccumulator += abs(energyDelta_Wh);
    dischargedEnergyAccumulator_AllTime += abs(energyDelta_Wh);
    if (dischargedEnergyAccumulator >= 1.0f) {
      DischargedEnergy += (int)dischargedEnergyAccumulator;
      dischargedEnergyAccumulator -= (int)dischargedEnergyAccumulator;
    }
    if (dischargedEnergyAccumulator_AllTime >= 1.0f) {
      DischargedEnergy_AllTime += (int)dischargedEnergyAccumulator_AllTime;
      dischargedEnergyAccumulator_AllTime -= (int)dischargedEnergyAccumulator_AllTime;
    }
  }

  // =================================================================
  //     COULOMB COUNTING - BATTERY STATE OF CHARGE TRACKING
  // =================================================================
  // Track actual amp-hours into/out of battery with efficiency corrections

  static float coulombAccumulator_Ah = 0.0f;
  float batteryCurrent_A = BatteryCurrent_scaled / 100.0f;
  float deltaAh = (batteryCurrent_A * elapsedSeconds) / 3600.0f;  // A × hours = Ah
  if (BatteryCurrent_scaled >= 0) {
    // Charging - apply charge efficiency (not all energy makes it in)
    // ChargeEfficiency_scaled is % × 10 (e.g. 990 = 99.0%), so divide by 1000 to get the multiplier.
    float batteryDeltaAh = deltaAh * (ChargeEfficiency_scaled / 1000.0f);
    coulombAccumulator_Ah += batteryDeltaAh;
  } else {
    // Discharging - apply Peukert compensation for high discharge rates
    float dischargeCurrent_A = abs(batteryCurrent_A);
    float peukertThreshold = BatteryCapacity_Ah / 100.0f;  // C/100 threshold

    if (dischargeCurrent_A > peukertThreshold) {
      // High discharge rate - apply Peukert equation
      float peukertExponent = PeukertExponent_scaled / 100.0f;
      dischargeCurrent_A = constrain(dischargeCurrent_A, 0, BatteryCapacity_Ah);  // Max 1C
      float currentRatio = PeukertRatedCurrent_A / dischargeCurrent_A;
      float peukertFactor = pow(currentRatio, peukertExponent - 1.0f);
      peukertFactor = constrain(peukertFactor, 0.5f, 2.0f);  // Sanity limits
      float batteryDeltaAh = deltaAh * peukertFactor;
      coulombAccumulator_Ah += batteryDeltaAh;
    } else {
      // Low discharge rate - no Peukert compensation needed
      coulombAccumulator_Ah += deltaAh;
    }
  }

  // Update SoC when we've accumulated enough change (0.01 Ah threshold)
  if (abs(coulombAccumulator_Ah) >= 0.01f) {
    int deltaAh_scaled = (int)(coulombAccumulator_Ah * 100.0f);
    CoulombCount_Ah_scaled += deltaAh_scaled;
    coulombAccumulator_Ah -= (deltaAh_scaled / 100.0f);  // Keep remainder
  }

  // Calculate State of Charge percentage with bounds checking
  CoulombCount_Ah_scaled = constrain(CoulombCount_Ah_scaled, 0, BatteryCapacity_Ah * 100);
  float SoC_float = (float)CoulombCount_Ah_scaled / (BatteryCapacity_Ah * 100.0f) * 100.0f;
  SOC_percent = (int)(SoC_float * 100);  // Store as percentage × 100 for 2 decimals
  wmIgnUpdate(wmIgn_SOC, SoC_float);     // ignition-cycle watermark (float percent, 0..100)

  // Battery Health: track the cycle's deepest point for the capacity extrapolation below.
  if (CoulombCount_Ah_scaled < bhCycleMinCoulomb_scaled) {
    bhCycleMinCoulomb_scaled = CoulombCount_Ah_scaled;
    bhCycleMinSoC_x100 = SOC_percent;
  }

  // =================================================================
  //     FULL CHARGE DETECTION - WORKS FROM ANY CHARGING SOURCE
  // =================================================================
  // When battery voltage is high AND current is low (tail current),
  // we know it's fully charged regardless of what's charging it
  // Units: BatteryCurrent_scaled is A×100. TailCurrent is % of capacity, so the
  // threshold in A×100 is (TailCurrent/100 × Capacity) × 100 = TailCurrent × Capacity.
  // (The old "/ 100" compared amps against A×100 — 100x too strict, never fired.)
  if ((abs(BatteryCurrent_scaled) <= (TailCurrent * BatteryCapacity_Ah)) && (Voltage_scaled >= ChargedVoltage_Scaled)) {
    // Conditions met - start/continue timer
    FullChargeTimer += elapsedSeconds;

    if (FullChargeTimer >= ChargedDetectionTime) {
      // Timer expired - battery is definitely full
      SOC_percent = 10000;  // 100.00%
      CoulombCount_Ah_scaled = BatteryCapacity_Ah * 100;

      static unsigned long lastFullChargeMessage = 0;
      if (!FullChargeDetected || millis() - lastFullChargeMessage > 60000) {
        char msg[128];
        queueConsoleMessageF("BATTERY: Full charge detected - SoC reset to 100%% (V=%.2fV >= %.2fV, I=%.2fA, Timer=%.1fs)",
                             Voltage_scaled / 100.0, ChargedVoltage_Scaled / 100.0,
                             BatteryCurrent_scaled / 100.0, FullChargeTimer);
        lastFullChargeMessage = millis();
      }

      // Battery Health capacity point, once per full-charge event (gated by !FullChargeDetected).
      // Admit only deep cycles (started ≤80% SoC) — shallow top-offs are too noisy.
      if (!FullChargeDetected) {
        float fullAh   = (float)BatteryCapacity_Ah;             // CoulombCount was just reset to full
        float minAh    = bhCycleMinCoulomb_scaled / 100.0f;
        float socStart = bhCycleMinSoC_x100 / 100.0f;          // percent
        float ahAdded  = fullAh - minAh;
        if (socStart <= 80.0f && ahAdded > 0.0f) {
          float depth = (100.0f - socStart) / 100.0f;
          if (depth > 0.05f) {
            float capacityEst = ahAdded / depth;
            if (capacityEst > 0.3f * BatteryCapacity_Ah && capacityEst < 3.0f * BatteryCapacity_Ah) {
              bhAppendCapacityPoint(capacityEst, socStart);
            }
          }
        }
        bhCycleMinCoulomb_scaled = (float)BatteryCapacity_Ah * 100.0f;  // reset cycle trackers to full
        bhCycleMinSoC_x100 = 10000;
      }

      FullChargeDetected = true;
      coulombAccumulator_Ah = 0.0f;

      // Apply shunt gain correction — battery current always comes from INA228
      if (AutoShuntGainCorrection == 1) {
        applySocGainCorrection();
      }
    }
  } else {
    // Conditions not met - reset timer
    FullChargeTimer = 0;
    FullChargeDetected = false;
  }

  // =================================================================
  //     ALTERNATOR-SPECIFIC TRACKING - ONLY RUNS WHEN ALT IS ON
  // =================================================================
  // These metrics are ONLY about the alternator's contribution

  // Determine if alternator is actually producing current
  alternatorIsOn = (MeasuredAmps > CurrentThreshold);

  if (alternatorIsOn) {
    // Track alternator runtime (seconds → AlternatorOnTime)
    alternatorOnAccumulator += elapsedMillis;
    if (alternatorOnAccumulator >= 1000) {  // Every 1 second
      int secondsRun = alternatorOnAccumulator / 1000;
      AlternatorOnTime += secondsRun;
      AlternatorOnTime_AllTime += secondsRun;
      alternatorOnAccumulator %= 1000;  // Keep remainder milliseconds
    }

    alternatorWasOn = alternatorIsOn;  // State tracking

    // Calculate alternator energy output
    static float alternatorEnergyAccumulator = 0.0f;
    static float alternatorEnergyAccumulator_AllTime = 0.0f;
    float alternatorPower_W = (currentBatteryVoltage * MeasuredAmps);
    if (alternatorPower_W > alt_power_max_alltime_w) alt_power_max_alltime_w = alternatorPower_W;  // leaderboard: peak alt power
    float altEnergyDelta_Wh = (alternatorPower_W * elapsedSeconds) / 3600.0f;

    if (altEnergyDelta_Wh > 0) {
      alternatorEnergyAccumulator += altEnergyDelta_Wh;
      alternatorEnergyAccumulator_AllTime += altEnergyDelta_Wh;
      if (alternatorEnergyAccumulator >= 1.0f) {
        AlternatorChargedEnergy += (int)alternatorEnergyAccumulator;
        alternatorEnergyAccumulator -= (int)alternatorEnergyAccumulator;
      }
      if (alternatorEnergyAccumulator_AllTime >= 1.0f) {
        AlternatorChargedEnergy_AllTime += (int)alternatorEnergyAccumulator_AllTime;
        alternatorEnergyAccumulator_AllTime -= (int)alternatorEnergyAccumulator_AllTime;
      }
      // =================================================================
      //     FUEL CONSUMPTION CALCULATION - ALTERNATOR SPECIFIC
      // =================================================================
      // Calculate diesel fuel used to produce this electrical energy
      // Chain: Diesel → Engine (30% eff) → Alternator (50% eff) → Electricity

      float energyJoules = altEnergyDelta_Wh * 3600.0f;  // Convert Wh to Joules
      const float engineEfficiency = 0.30f;              // Engine: fuel → mechanical (30%)
      const float alternatorEfficiency = 0.50f;          // Alt: mechanical → electrical (50%)

      // Total system efficiency = 0.30 × 0.50 = 0.15 (15%)
      float fuelEnergyUsed_J = energyJoules / (engineEfficiency * alternatorEfficiency);
      const float dieselEnergy_J_per_mL = 36000.0f;  // Energy content of diesel

      float fuelUsed_mL = fuelEnergyUsed_J / dieselEnergy_J_per_mL;
      float fuelUsed_L = fuelUsed_mL / 1000.0f;  // Convert mL to L

      // Accumulate fuel (prevents losing fractional L)
      static float fuelAccumulator = 0.0f;
      static float fuelAccumulator_AllTime = 0.0f;
      fuelAccumulator += fuelUsed_L;
      fuelAccumulator_AllTime += fuelUsed_L;

      if (fuelAccumulator >= 0.01f) {
        float toAdd = floor(fuelAccumulator * 100.0f) / 100.0f;  // Round down to nearest 0.01 L
        AlternatorFuelUsed += toAdd;
        fuelAccumulator -= toAdd;  // Keep the fractional remainder
      }
      if (fuelAccumulator_AllTime >= 0.01f) {
        float toAdd = floor(fuelAccumulator_AllTime * 100.0f) / 100.0f;
        AlternatorFuelUsed_AllTime += toAdd;
        fuelAccumulator_AllTime -= toAdd;  // Keep the fractional remainder
      }
    }
  }

  // =================================================================
  //     CHARGE CYCLE CALCULATION
  // =================================================================
  // Calculate charge cycles based on total energy throughput
  // One cycle = one full battery capacity worth of energy charged
  // Use the user-entered nominal bank class (BATTERY_VOLTAGE) directly — this runs continuously, well
  // after InitSystemSettings has loaded it, so there's no need to (mis)guess the class from the measured
  // voltage. A deeply-discharged 24V bank sagging below 16V no longer mis-buckets as 12V (2× cycle error).
  float nominalVoltage = (float)BATTERY_VOLTAGE;

  float batteryCapacity_Wh = BatteryCapacity_Ah * nominalVoltage;

  if (batteryCapacity_Wh > 0) {
    ChargeCycles = (ChargedEnergy / batteryCapacity_Wh);  //Units: Wh
    ChargeCycles_AllTime = (ChargedEnergy_AllTime) / batteryCapacity_Wh;
  }

  // =================================================================
  //     AVERAGE SOC TRACKING (TIME-WEIGHTED)
  // =================================================================
  // Time-weighted average SOC calculation
  static float socAccumulator = 0.0f;
  // socAccumulator_AllTime is now GLOBAL (not static here)
  // totalSocSampleTime_AllTime is now GLOBAL (not static here)
  static unsigned long totalSocSampleTime = 0;  // Session seconds tracked

  float currentSOC = SOC_percent / 100.0f;  // Convert to actual percentage

  socAccumulator += currentSOC * elapsedSeconds;
  socAccumulator_AllTime += currentSOC * elapsedSeconds;
  totalSocSampleTime += elapsedSeconds;
  totalSocSampleTime_AllTime += elapsedSeconds;

  if (totalSocSampleTime > 0) {
    AvgSOC = socAccumulator / totalSocSampleTime;
  }
  if (totalSocSampleTime_AllTime > 0) {
    AvgSOC_AllTime = socAccumulator_AllTime / totalSocSampleTime_AllTime;
  }


  // Check calculation
  if (totalSocSampleTime_AllTime > 0) {
    float calculatedAvg = socAccumulator_AllTime / totalSocSampleTime_AllTime;
  } else {
    Serial.println("WARNING: totalSocSampleTime_AllTime is ZERO!");
  }


  // Track voltage sampling time
  totalVoltageSampleTime_AllTime += elapsedSeconds;

  // Calculate time-weighted average voltage
  // voltageAccumulator_AllTime = 0.0f; don't do this shit anymore
  float currentVoltage = getBatteryVoltage();
  voltageAccumulator_AllTime += currentVoltage * elapsedSeconds;

  if (totalVoltageSampleTime_AllTime > 0) {
    AvgVoltage_AllTime = voltageAccumulator_AllTime / totalVoltageSampleTime_AllTime;
  }
}
void UpdateTravelStatistics(unsigned long elapsedMillis) {
  // ==========================================================================
  // DISTANCE CALCULATION - Using GPS position (Haversine)
  // ==========================================================================

  // gpsValid is shared with trip-tracking below — spec criteria: not stale,
  // not NaN, not (0,0). Stricter than the old IS_STALE-only gate; the extra
  // checks reject the "boot before first fix" sentinel.
  bool gpsValid = !IS_STALE(IDX_LATITUDE_NMEA) && !IS_STALE(IDX_LONGITUDE_NMEA)
                  && !isnan(LatitudeNMEA) && !isnan(LongitudeNMEA)
                  && !(LatitudeNMEA == 0.0 && LongitudeNMEA == 0.0);
  float tripDistanceDelta_nm = 0.0f;  // jump-filtered delta for the trip accumulator

  if (gpsValid) {
    static double lastLat = 0;
    static double lastLon = 0;
    static uint32_t lastFixMs = 0;
    static bool firstRun = true;

    if (firstRun) {
      lastLat = LatitudeNMEA;
      lastLon = LongitudeNMEA;
      lastFixMs = millis();
      firstRun = false;
    } else {
      double distanceDelta_nm = calculateHaversineDistance(lastLat, lastLon, LatitudeNMEA, LongitudeNMEA);

      // Only act when the fix actually moved. Position is re-read every 2 s but only
      // refreshes every 60 s on phone GPS, so most ticks see an identical fix (delta 0).
      if (distanceDelta_nm > 0.0) {
        uint32_t nowMs = millis();
        uint32_t dtMs = nowMs - lastFixMs;
        float impliedKn = (dtMs > 0) ? (float)(distanceDelta_nm / (dtMs / 3600000.0)) : 9999.0f;

        // Implied-speed gate replaces the old fixed 0.1 nm jump filter, which dropped every
        // step above ~6 kt on 60 s phone-GPS fixes (zeroing distance for fast boats). A
        // <=5 min gap at <150 kt is real motion (covers ~100 mph powerboats; real GPS teleports
        // imply hundreds of kt); anything else rebaselines without polluting the odometer.
        if (dtMs <= 300000UL && impliedKn < 150.0f) {
          static float distanceAccumulator = 0.0f;
          static float distanceAccumulator_AllTime = 0.0f;
          static float sailingDistAccumulator_AllTime = 0.0f;  // engine-off miles (leaderboard)

          distanceAccumulator += (float)distanceDelta_nm;
          distanceAccumulator_AllTime += (float)distanceDelta_nm;
          if (RPM < 50 && Ignition == 0) sailingDistAccumulator_AllTime += (float)distanceDelta_nm;  // under sail = engine off (matches sailing_days gate)

          if (distanceAccumulator >= 0.01f) {
            TotalDistance += distanceAccumulator;
            distanceAccumulator = 0.0f;
          }
          if (distanceAccumulator_AllTime >= 0.01f) {
            TotalDistance_AllTime += distanceAccumulator_AllTime;
            distanceAccumulator_AllTime = 0.0f;
          }
          if (sailingDistAccumulator_AllTime >= 0.01f) {
            sailing_dist_alltime += sailingDistAccumulator_AllTime;
            sailingDistAccumulator_AllTime = 0.0f;
          }

          tripDistanceDelta_nm = (float)distanceDelta_nm;  // reuse validated delta for trip
        }

        lastLat = LatitudeNMEA;
        lastLon = LongitudeNMEA;
        lastFixMs = nowMs;
      }
    }
  }

  // ==========================================================================
  // LONGEST SINGLE TRIP TRACKING
  // Trip ends after 60 min continuous (a) GPS invalid OR (b) SOG < 1.5 kn.
  // Either valid GPS + SOG >= 1.5 kn sample resets both timers.
  // Boot recovery: if NVS had an in-progress trip, hold in stasis until time
  // syncs, then resume (epoch < 1 hr old) or finalize (stale). 10-min fallback
  // forces stale-finalize if time never syncs (e.g., no GPS / no NTP).
  // ==========================================================================
  {
    if (tripPendingRecovery) {
      if (timeIsSynced && timeBase > 0) {
        uint32_t nowEpoch = timeBase + (millis() - timeBaseMillis) / 1000;
        bool fresh = (currentTripLastUpdateEpoch > 0)
                  && (nowEpoch >= currentTripLastUpdateEpoch)
                  && ((nowEpoch - currentTripLastUpdateEpoch) < 3600UL);
        if (fresh) {
          tripActive = true;
        } else {
          if (currentTripDistanceNm > LongestSingleTrip_Nm_AllTime) LongestSingleTrip_Nm_AllTime = currentTripDistanceNm;
          tripActive = false;
          currentTripDistanceNm = 0.0f;
          currentTripLastUpdateEpoch = 0;
        }
        tripPendingRecovery = false;
      } else if (millis() > 600000UL) {
        // 10 min since boot without a time sync — finalize conservatively so the saved trip isn't held in limbo forever.
        if (currentTripDistanceNm > LongestSingleTrip_Nm_AllTime) LongestSingleTrip_Nm_AllTime = currentTripDistanceNm;
        tripActive = false;
        currentTripDistanceNm = 0.0f;
        currentTripLastUpdateEpoch = 0;
        tripPendingRecovery = false;
      }
    }

    if (!tripPendingRecovery) {
      bool sogValid = !IS_STALE(IDX_SOG_NMEA) && SOGNMEA >= 0;
      float sog_kn = sogValid ? SOGNMEA : 0.0f;  // stale SOG counts as no motion

      if (gpsValid && sog_kn >= 1.5f) {
        timeSinceLastMotion = 0;
        timeSinceLastValidGps = 0;
        if (!tripActive) {
          tripActive = true;
          currentTripDistanceNm = 0.0f;
        }
        currentTripDistanceNm += tripDistanceDelta_nm;
        // Stamp wall-clock epoch (project pattern, since GPS/phone time sources don't touch system clock).
        // Leaving as 0 when time isn't synced means boot recovery conservatively finalizes the trip.
        if (timeIsSynced) currentTripLastUpdateEpoch = timeBase + (millis() - timeBaseMillis) / 1000;
      } else {
        if (!gpsValid)     timeSinceLastValidGps += elapsedMillis;
        else               timeSinceLastValidGps = 0;
        if (sog_kn < 1.5f) timeSinceLastMotion   += elapsedMillis;
        else               timeSinceLastMotion   = 0;

        if (tripActive && (timeSinceLastMotion >= 3600000UL || timeSinceLastValidGps >= 3600000UL)) {
          if (currentTripDistanceNm > LongestSingleTrip_Nm_AllTime) {
            LongestSingleTrip_Nm_AllTime = currentTripDistanceNm;
          }
          tripActive = false;
          currentTripDistanceNm = 0.0f;
        }
      }
    }
  }

  // ==========================================================================
  // ROLLING 24-HOUR MAX DISTANCE (lb-max-24hr leaderboard)
  // Ring of 24 hourly buckets; bucket[head] = in-progress hour. Sum = approx
  // last 24 hr of motion (~1 hr undercount at the leading edge).
  // ==========================================================================
  {
    uint32_t now = millis();
    if (distHourStartMs == 0) distHourStartMs = now;  // first-tick init
    while ((uint32_t)(now - distHourStartMs) >= 3600000UL) {
      distHourHead = (distHourHead + 1) % 24;
      distHourBuckets[distHourHead] = 0.0f;
      distHourStartMs += 3600000UL;
    }
    distHourBuckets[distHourHead] += tripDistanceDelta_nm;
    float rollingSum = 0.0f;
    for (uint8_t i = 0; i < 24; i++) rollingSum += distHourBuckets[i];
    if (rollingSum > Max24hrDistance_AllTime) Max24hrDistance_AllTime = rollingSum;
  }

  // Anchorage detection — see UpdateAnchorageDetection() for the full algorithm.
  UpdateAnchorageDetection(gpsValid);

  // ==========================================================================
  // SPEED CALCULATION - Using SOGNMEA (time-weighted average) - UNCHANGED
  // ==========================================================================

  if (IS_STALE(IDX_SOG_NMEA) || SOGNMEA < 0) {
    return;
  }

  static float speedAccumulator = 0.0f;
  static unsigned long totalSpeedSampleTime = 0;  // Session seconds

  float elapsedSeconds = elapsedMillis / 1000.0f;

  speedAccumulator += (float)(SOGNMEA * elapsedSeconds);
  speedAccumulator_AllTime += (float)(SOGNMEA * elapsedSeconds);
  totalSpeedSampleTime += (unsigned long)elapsedSeconds;
  totalSpeedSampleTime_AllTime += (unsigned long)elapsedSeconds;

  if (totalSpeedSampleTime > 0) {
    AvgSpeed = speedAccumulator / (float)totalSpeedSampleTime;
  }
  if (totalSpeedSampleTime_AllTime > 0) {
    AvgSpeed_AllTime = speedAccumulator_AllTime / (float)totalSpeedSampleTime_AllTime;
  }
}

// Anchorage detection — pushes one sample per minute into the PSRAM ring while GPS+depth are
// both valid. Each push evaluates the trailing sliding 5-hr span; if continuous (no gaps > 10 min)
// and qualifying (swing < 100 yd, depth varies 2-100 ft), updates DeepestAnchorage_Ft_AllTime
// with the window's average depth.
void UpdateAnchorageDetection(bool gpsValid) {
  if (!anchorageRing) return;
  const uint32_t SAMPLE_INTERVAL_MS  = 60000UL;            // 1-minute sample cadence
  const uint32_t WINDOW_SPAN_MS      = 5UL * 3600UL * 1000UL; // 5 hours
  const uint32_t GAP_TOLERANCE_MS    = 10UL * 60UL * 1000UL;  // 10 min reboot/dropout tolerance
  const double   MAX_SWING_NM        = 0.0494;             // ~100 yards
  const float    MIN_DEPTH_CHANGE_FT = 2.0f;
  const float    MAX_DEPTH_CHANGE_FT = 100.0f;
  uint32_t now = millis();
  if ((uint32_t)(now - lastAnchorageSampleMs) < SAMPLE_INTERVAL_MS) return;
  // Only sample when we have BOTH valid GPS AND fresh depth — otherwise the window can't qualify.
  if (!gpsValid || IS_STALE(IDX_WATER_DEPTH)) {
    lastAnchorageSampleMs = now;  // throttle gate; skip this sample (gap will be detected on resume)
    return;
  }
  anchorageRing[anchorageRingHead].sampleMs = now;
  anchorageRing[anchorageRingHead].lat = LatitudeNMEA;
  anchorageRing[anchorageRingHead].lon = LongitudeNMEA;
  anchorageRing[anchorageRingHead].depth_ft = WaterDepth_m * 3.28084f;
  anchorageRingHead = (anchorageRingHead + 1) % ANCHORAGE_RING_SIZE;
  if (anchorageRingCount < ANCHORAGE_RING_SIZE) anchorageRingCount++;
  lastAnchorageSampleMs = now;
  // Evaluate trailing window. Walk backward from newest, collecting samples until we hit a
  // gap > GAP_TOLERANCE_MS or run out of samples. Need at least WINDOW_SPAN_MS of contiguous coverage.
  if (anchorageRingCount < 2) return;
  uint16_t newestIdx = (anchorageRingHead + ANCHORAGE_RING_SIZE - 1) % ANCHORAGE_RING_SIZE;
  uint32_t newestMs = anchorageRing[newestIdx].sampleMs;
  uint16_t spanCount = 1;
  uint16_t oldestValidIdx = newestIdx;
  for (uint16_t step = 1; step < anchorageRingCount; step++) {
    uint16_t prevIdx = (newestIdx + ANCHORAGE_RING_SIZE - step) % ANCHORAGE_RING_SIZE;
    uint16_t laterIdx = (newestIdx + ANCHORAGE_RING_SIZE - step + 1) % ANCHORAGE_RING_SIZE;
    uint32_t gap = (uint32_t)(anchorageRing[laterIdx].sampleMs - anchorageRing[prevIdx].sampleMs);
    if (gap > GAP_TOLERANCE_MS) break;  // contiguous span ends here
    oldestValidIdx = prevIdx;
    spanCount++;
  }
  uint32_t spanMs = (uint32_t)(newestMs - anchorageRing[oldestValidIdx].sampleMs);
  if (spanMs < WINDOW_SPAN_MS) return;  // not yet 5 contiguous hours
  // Compute centroid + depth stats over the valid trailing span.
  double sumLat = 0, sumLon = 0;
  float depthMin = 1e9f, depthMax = -1e9f, depthSum = 0.0f;
  for (uint16_t step = 0; step < spanCount; step++) {
    uint16_t idx = (newestIdx + ANCHORAGE_RING_SIZE - step) % ANCHORAGE_RING_SIZE;
    sumLat += anchorageRing[idx].lat;
    sumLon += anchorageRing[idx].lon;
    float d = anchorageRing[idx].depth_ft;
    if (d < depthMin) depthMin = d;
    if (d > depthMax) depthMax = d;
    depthSum += d;
  }
  double centroidLat = sumLat / (double)spanCount;
  double centroidLon = sumLon / (double)spanCount;
  float depthAvg = depthSum / (float)spanCount;
  float depthRange = depthMax - depthMin;
  // Max distance from centroid (Haversine).
  double maxSwingNm = 0;
  for (uint16_t step = 0; step < spanCount; step++) {
    uint16_t idx = (newestIdx + ANCHORAGE_RING_SIZE - step) % ANCHORAGE_RING_SIZE;
    double d = calculateHaversineDistance(centroidLat, centroidLon,
                                          anchorageRing[idx].lat, anchorageRing[idx].lon);
    if (d > maxSwingNm) maxSwingNm = d;
  }
  if (maxSwingNm > MAX_SWING_NM) return;
  if (depthRange < MIN_DEPTH_CHANGE_FT || depthRange > MAX_DEPTH_CHANGE_FT) return;
  // Qualifying anchorage — update lifetime watermark.
  if (depthAvg > DeepestAnchorage_Ft_AllTime) DeepestAnchorage_Ft_AllTime = depthAvg;
}

void UpdateEngineRuntime(unsigned long elapsedMillis) {
  // Check if engine is running (RPM > 100)
  bool engineIsRunning = (RPM > 100 && RPM < 6000);

  if (engineIsRunning) {
    // Accumulate running time in milliseconds
    engineRunAccumulator += elapsedMillis;

    // Update total engine run time every second
    if (engineRunAccumulator >= 1000) {  // 1 second in milliseconds
      int secondsRun = engineRunAccumulator / 1000;
      EngineRunTime += secondsRun;
      EngineRunTime_AllTime += secondsRun;

      // Update engine cycles (RPM * seconds / 60)
      int cyclesDelta = (RPM * secondsRun) / 60;
      EngineCycles += cyclesDelta;
      EngineCycles_AllTime += cyclesDelta;

      // Keep the remainder milliseconds
      engineRunAccumulator %= 1000;
    }
  }

  // Engine start/stop edge — one console line per transition. Anchors the session
  // timeline so charging events can be read against engine state.
  if (engineIsRunning && !engineWasRunning) {
    queueConsoleMessageF("Engine STARTED (RPM=%d)", (int)RPM);
  } else if (!engineIsRunning && engineWasRunning) {
    queueConsoleMessageF("Engine STOPPED (RPM=%d)", (int)RPM);
  }

  // Update engine state
  engineWasRunning = engineIsRunning;
}
// Function to get smoothed GPS position (5-sample moving average)
void getSmoothedGPS(double &smoothLat, double &smoothLon) {
  if (gpsBufferCount == 0) {
    smoothLat = LatitudeNMEA;
    smoothLon = LongitudeNMEA;
    return;
  }

  double latSum = 0;
  double lonSum = 0;
  int samplesToAverage = min(gpsBufferCount, GPS_SMOOTHING_SAMPLES);

  for (int i = 0; i < samplesToAverage; i++) {
    latSum += latBuffer[i];
    lonSum += lonBuffer[i];
  }

  smoothLat = latSum / samplesToAverage;
  smoothLon = lonSum / samplesToAverage;
}
// Call this whenever you get a new GPS position (in your GNSS handler or main loop)
void updateGPSBuffer() {
  // Only update if we have valid GPS data
  if (IS_STALE(IDX_LATITUDE_NMEA) || IS_STALE(IDX_LONGITUDE_NMEA)) {
    return;
  }

  // **FIX: On first valid reading, pre-fill entire buffer**
  if (gpsBufferCount == 0) {
    for (int i = 0; i < GPS_SMOOTHING_SAMPLES; i++) {
      latBuffer[i] = LatitudeNMEA;
      lonBuffer[i] = LongitudeNMEA;
    }
    gpsBufferCount = GPS_SMOOTHING_SAMPLES;
    gpsBufferIndex = 0;
    Serial.println("GPS buffer initialized");
    return;
  }

  // Add to circular buffer
  latBuffer[gpsBufferIndex] = LatitudeNMEA;
  lonBuffer[gpsBufferIndex] = LongitudeNMEA;

  gpsBufferIndex = (gpsBufferIndex + 1) % GPS_SMOOTHING_SAMPLES;
}
// Calculate distance between two GPS coordinates using Haversine formula
double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 3440.065;  // Earth's radius in nautical miles

  // Convert to radians
  double lat1_rad = lat1 * PI / 180.0;
  double lat2_rad = lat2 * PI / 180.0;
  double delta_lat = (lat2 - lat1) * PI / 180.0;
  double delta_lon = (lon2 - lon1) * PI / 180.0;

  // Haversine formula
  double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) + cos(lat1_rad) * cos(lat2_rad) * sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

  return R * c;  // Distance in nautical miles
}

/**
 * UpdateSailingMetrics - Track sailing time and calculate ratio
 * Sailing conditions: SOG > 0.5 knots AND RPM < 50 AND Ignition == 0
 */
void UpdateSailingMetrics(unsigned long elapsedMillis) {
  // Check sailing conditions
  bool isSailing = false;
  if (!IS_STALE(IDX_SOG_NMEA) && SOGNMEA > 0.5 && RPM < 50 && Ignition == 0) {
    isSailing = true;
  }

  // Update total operational time (always, regardless of sailing)
  float elapsedSeconds = elapsedMillis / 1000.0f;

  // Update sailing time (only when sailing)
  if (isSailing) {
    float elapsedDays = elapsedMillis / (1000.0f * 60.0f * 60.0f * 24.0f);

    static float sailingDaysAccumulator = 0.0f;
    sailingDaysAccumulator += elapsedDays;

    // Update when accumulated at least 0.001 days (~1.4 minutes)
    if (sailingDaysAccumulator >= 0.001f) {
      sailing_days_alltime += sailingDaysAccumulator;
      sailingDaysAccumulator = 0.0f;
      // Will be saved to NVS every 2 minutes
    }
  }

  // Calculate sailing ratio using existing totalSocSampleTime_AllTime
  float total_operational_days = totalSocSampleTime_AllTime / 86400.0f;  // ✅ Use existing variable
  if (total_operational_days > 0) {
    sailing_ratio = (sailing_days_alltime / total_operational_days) * 100.0f;
  } else {
    sailing_ratio = 0.0f;
  }
}
void UpdateWindMaximums() {
  if (!IS_STALE(IDX_APPARENT_WIND_SPEED) && ApparentWindSpeedNMEA > max_wind_speed_apparent_alltime) {
    max_wind_speed_apparent_alltime = ApparentWindSpeedNMEA;
  }
  if (!isnan(TrueWindSpeedNMEA) && TrueWindSpeedNMEA > max_wind_speed_true_alltime) {
    max_wind_speed_true_alltime = TrueWindSpeedNMEA;
  }
  // ignition-cycle watermarks (lo + hi, separate from *_alltime above)
  if (!IS_STALE(IDX_APPARENT_WIND_SPEED)) wmIgnUpdate(wmIgn_AWS, ApparentWindSpeedNMEA);
  if (!isnan(TrueWindSpeedNMEA))          wmIgnUpdate(wmIgn_TWS, TrueWindSpeedNMEA);
}

void UpdateBoardTempPressureMaximums() {
  // Guard per-field — NaN compares false against everything, but watermark updaters
  // shouldn't ingest NaN. A BMP388 read failure could surface NaN on either channel.
  if (!isnan(ambientTemp)) {
    if (ambientTemp > board_temp_max_alltime)        board_temp_max_alltime    = ambientTemp;
    if (ambientTemp < board_temp_min_alltime)        board_temp_min_alltime    = ambientTemp;
    wmIgnUpdate(wmIgn_ambient, ambientTemp);
  }
  if (!isnan(baroPressure)) {
    if (baroPressure > baro_pressure_max_alltime)    baro_pressure_max_alltime = baroPressure;
    if (baroPressure < baro_pressure_min_alltime)    baro_pressure_min_alltime = baroPressure;
    wmIgnUpdate(wmIgn_baro, baroPressure);
  }
}


float getBatteryCurrent() {
  // Return battery current from appropriate source
  // Variables are populated by ReadAnalogInputs() or ReadAnalogInputs_Fake()
  switch (BatteryCurrentSource) {
    case 3: return VictronCurrent;  // Victron VE.Direct
    default: return Bcur;           // INA228 (case 0, and fallback)
  }
}
float getBatteryVoltage() {
  // INA228 bus voltage — more accurate (20-bit, no divider drift) and faster in field-on mode.
  // BatteryV (ADS1115) is retained separately for the cross-sensor disagreement check only.
  return IBV;
}
float getTargetAmps() {
  return MeasuredAmps;
}

float getFiltI() {
  return MeasuredAmps_filtered;
}
float getFiltV() {
  // Never use for safety checks — use IBV directly.
  return IBV_filtered;
}

// Channel 3 topology per docs/hardware/analoginputsADS1115.md:
//   3.3 V → Thermistor (R_NTC) → V_node → R_fixed (10 kΩ pulldown) → GND
//   V_node = 3.3 × R_fixed / (R_fixed + R_NTC)  →  R_NTC = R_fixed × (Vcc - V) / V
// Earlier firmware assumed the inverted topology with Vcc=5V and produced wrong R_NTC.
float thermistorTempC(float V_node) {
  const float Vcc = 3.3f;
  if (V_node <= 0.0f || V_node >= Vcc) return -99.0f;  // unrecoverable: divide-by-zero or rail
  float R_NTC = R_fixed * (Vcc - V_node) / V_node;
  float T0_K = T0_C + 273.15f;
  float tempK = 1.0f / ((1.0f / T0_K) + (1.0f / Beta) * log(R_NTC / R0));
  return tempK - 273.15f;
}

//============================================================================
// CheckAlarms() - Alarm monitoring and INA228 hardware protection
// ============================================================================
void CheckAlarms() {
  static unsigned long lastRunTime = 0;
  if (millis() - lastRunTime < 250) return;
  lastRunTime = millis();

  static bool previousAlarmState = false;
  bool currentAlarmCondition = false;
  bool outputAlarmState = false;
  const char *alarmReason = "";

  // ========== ALARM TEST MODE ==========
  if (AlarmTest == 1) {
    if (alarmTestStartTime == 0) {
      alarmTestStartTime = millis();
      queueConsoleMessage("ALARM TEST: Testing buzzer for 2 seconds");
    }

    if (millis() - alarmTestStartTime < ALARM_TEST_DURATION) {
      currentAlarmCondition = true;
      alarmReason = "Alarm test active";
    } else {
      AlarmTest = 0;
      alarmTestStartTime = 0;
      queueConsoleMessage("ALARM TEST: Complete");
    }
  }

  // ========== NORMAL ALARM CHECKING ==========
  if (AlarmActivate == 1) {
    if (TempSource == 0) {
      TempToUse = AlternatorTemperatureF;
    } else if (TempSource == 1) {
      TempToUse = temperatureThermistor;
    }

    static unsigned long lastTempAlarmMsgMs = 0;
    if (TempAlarm > 0 && TempToUse > TempAlarm) {
      currentAlarmCondition = true;
      alarmReason = "High alternator temperature";
      if (millis() - lastTempAlarmMsgMs >= 30000) {
        lastTempAlarmMsgMs = millis();
        queueConsoleMessageF("High alternator temperature: %.1f°F (limit: %d°F)",
                             TempToUse, TempAlarm);
      }
    } else {
      lastTempAlarmMsgMs = 0;  // Reset so it fires immediately when condition returns
    }

    static unsigned long lastTempLowAlarmMsgMs = 0;
    if (TempAlarmLow > 0 && TempToUse < TempAlarmLow) {
      currentAlarmCondition = true;
      alarmReason = "Low alternator temperature";
      if (millis() - lastTempLowAlarmMsgMs >= 30000) {
        lastTempLowAlarmMsgMs = millis();
        queueConsoleMessageF("Low alternator temperature: %.1f°F (limit: %d°F)",
                             TempToUse, TempAlarmLow);
      }
    } else {
      lastTempLowAlarmMsgMs = 0;  // Reset so it fires immediately when condition returns
    }

    // Cold-charge lockout alarm — fail-open on stale/NaN board sensor, matching buildTickSnapshot().
    static unsigned long lastColdChargeAlarmMsgMs = 0;
    if (coldChargeLockoutEnable && !IS_STALE(IDX_AMBIENT_TEMP) && isfinite(ambientTemp) && ambientTemp < MinChargeTempF) {
      currentAlarmCondition = true;
      alarmReason = "Battery too cold to charge";
      if (millis() - lastColdChargeAlarmMsgMs >= 30000) {
        lastColdChargeAlarmMsgMs = millis();
        queueConsoleMessageF("Cold-charge lockout: board temp %.1f°F below %.0f°F floor — charging disabled",
                             ambientTemp, MinChargeTempF);
      }
    } else {
      lastColdChargeAlarmMsgMs = 0;  // Reset so it fires immediately when condition returns
    }

    // (Alternator-health is advisory-only now — no audible alarm. See Phase 2 redesign.)

    // Fast alt-current pulse-pattern fault (rectifier/stator) — opt-in audible alarm (item 5).
    // Sounds while a FAULT verdict is fresh (seen within the last 2 min); default OFF until
    // real-capture validation. AlarmActivate-gated like every other condition in this block.
    static unsigned long lastFaFaultAlarmMsgMs = 0;
    if (faAlarmEnable && faLastFaultMs != 0 && (millis() - faLastFaultMs < 120000UL)) {
      currentAlarmCondition = true;
      alarmReason = "Alternator current pulse-pattern fault";
      if (millis() - lastFaFaultAlarmMsgMs >= 30000) {
        lastFaFaultAlarmMsgMs = millis();
        queueConsoleMessageF("Alternator current pulse-pattern fault (class k=%u) -- check rectifier/stator", faDetectLastK);
      }
    } else {
      lastFaFaultAlarmMsgMs = 0;
    }

    float currentVoltage = getBatteryVoltage();

    static unsigned long lastVoltHighMsgMs = 0;
    if (VoltageAlarmHigh > 0 && currentVoltage > VoltageAlarmHigh) {
      currentAlarmCondition = true;
      alarmReason = "High battery voltage";
      if (millis() - lastVoltHighMsgMs >= 30000) {
        lastVoltHighMsgMs = millis();
        queueConsoleMessageF("High battery voltage: %.2fV (limit: %.0fV)",
                             currentVoltage, VoltageAlarmHigh);
      }
    } else {
      lastVoltHighMsgMs = 0;
    }

    static unsigned long lastVoltLowMsgMs = 0;
    // The > floor rejects a disconnected/0V reading; scale it by bank class (8V on 12V → 16/32V on
    // 24/48V) so it stays a "sensor disconnected" floor and never sits above a real low-V alarm point.
    if (VoltageAlarmLow > 0 && currentVoltage < VoltageAlarmLow && currentVoltage > 8.0 * BATTERY_VOLTAGE / 12.0f) {
      currentAlarmCondition = true;
      alarmReason = "Low battery voltage";
      if (millis() - lastVoltLowMsgMs >= 30000) {
        lastVoltLowMsgMs = millis();
        queueConsoleMessageF("Low battery voltage: %.2fV (limit: %.0fV)",
                             currentVoltage, VoltageAlarmLow);
      }
    } else {
      lastVoltLowMsgMs = 0;
    }

    static unsigned long lastCurHighMsgMs = 0;
    if (CurrentAlarmHigh > 0 && MeasuredAmps > CurrentAlarmHigh) {
      currentAlarmCondition = true;
      alarmReason = "High alternator current";
      if (millis() - lastCurHighMsgMs >= 30000) {
        lastCurHighMsgMs = millis();
        queueConsoleMessageF("High alternator current: %.1fA (limit: %.0fA)",
                             MeasuredAmps, CurrentAlarmHigh);
      }
    } else {
      lastCurHighMsgMs = 0;
    }

    static unsigned long lastBatCurMsgMs = 0;
    if (MaximumAllowedBatteryAmps > 0 && abs(Bcur) > MaximumAllowedBatteryAmps) {
      currentAlarmCondition = true;
      alarmReason = "High battery current";
      if (millis() - lastBatCurMsgMs >= 30000) {
        lastBatCurMsgMs = millis();
        queueConsoleMessageF("High battery current: %.1fA (limit: %.0fA)",
                             abs(Bcur), MaximumAllowedBatteryAmps);
      }
    } else {
      lastBatCurMsgMs = 0;
    }
  }

  // ========== TEMP TASK FAILURE ALARM ==========
  // Safety alarm — fires regardless of AlarmActivate. Throttled console reminder every 30s.
  if (tempTaskAlarm) {
    currentAlarmCondition = true;
    alarmReason = "TempTask failure — temperature monitoring lost";
    static unsigned long lastTempAlarmMsg = 0;
    if (millis() - lastTempAlarmMsg >= 30000) {
      lastTempAlarmMsg = millis();
      queueConsoleMessage("ALARM: TempTask not responding — temperature monitoring lost. Field reduced as safety measure.");
    }
  }

  // ========== TEMP DATA STALE ALARM ==========
  // Fires when no valid reading for 20s (sensor disconnected or persistent comm failure).
  // Gated by AlarmActivate. Complements the field cut that fires at the same threshold.
  if (AlarmActivate == 1) {
    uint32_t tempIdx = (TempSource == 0) ? IDX_ALTERNATOR_TEMP : IDX_THERMISTOR_TEMP;
    uint32_t ts = dataTimestamps[tempIdx];
    bool tempStale = (ts != 0 && (millis() - ts) > 20000) ||
                     (ts == 0 && millis() > 60000);  // never-connected grace period matches tick logic
    static unsigned long lastTempStaleAlarmMsg = 0;
    if (tempStale) {
      currentAlarmCondition = true;
      alarmReason = "Temperature sensor not responding";
      if (millis() - lastTempStaleAlarmMsg >= 30000) {
        lastTempStaleAlarmMsg = millis();
        queueConsoleMessage("ALARM: No temperature reading for 20s — sensor may be disconnected.");
      }
    } else {
      lastTempStaleAlarmMsg = 0;
    }
  }

  // ========== INA228 HARDWARE OVERVOLTAGE PROTECTION ==========
  // The INA228 ALERT pin pulls GPIO4 low electrically the instant the bus
  // voltage crosses VoltageHardwareLimit — before any software runs.
  // This block manages the software latch that keeps the field suppressed
  // until the condition is confirmed cleared, and produces clear messaging
  // about what the hardware did and when it recovered.
  //
  // SLOW_ALERT is SET: the threshold comparison uses the averaged ADC value
  // (~1054ms filter with 128-sample averaging), not instantaneous readings.
  // This prevents single-sample noise spikes from asserting the ALERT pin.
  //
  // New event detection: throttled to every 5s to avoid hammering I2C.
  // Latch management (3s and 10s checks): runs every 250ms.

  if (INADisconnected == 0) {
    static unsigned long lastINA228Check = 0;
    uint16_t alertStatus = 0;

    // ── Latch management — runs every 250ms while latched ────────────────
    if (inaOvervoltageLatched) {
      uint32_t latchAge = millis() - inaOvervoltageTime;
      static bool earlyCheckDone = false;

      currentAlarmCondition = true;
      alarmReason = "INA228 hardware overvoltage";

      // Periodic reminder while latch is held — every 5s to avoid spam
      static unsigned long lastOVMessage = 0;
      if (millis() - lastOVMessage >= 5000) {
        lastOVMessage = millis();
        float busV = INA.getBusVoltage();
        queueConsoleMessageF(
          "INA228 OV LATCH HELD [%lus]: busV=%.3fV limit=%.3fV | "
          "ALERT pin cut GPIO4 electrically. Field suppressed by SW latch. "
          "Checking for clear at 3s and 10s.",
          latchAge / 1000, busV, VoltageHardwareLimit);
      }

      // ── 3s early check: fast release for genuine transient spikes ────
      if (!earlyCheckDone && latchAge >= 3000) {
        earlyCheckDone = true;
        clearINA228AlertLatch(INA.getAddress());
        alertStatus = readINA228AlertRegister(INA.getAddress());
        float busV = INA.getBusVoltage();

        // I²C error (0xFFFF) → treat as "still set", hold latch (fail-safe)
        if (alertStatus != 0xFFFF && !(alertStatus & 0x0010)) {
          // Cleared — was a transient. Release latch now.
          inaOvervoltageLatched = false;
          inaOvervoltageClearedMs = millis();
          earlyCheckDone = false;
          queueConsoleMessageF(
            "INA228 OV CLEARED [3s early check]: busV=%.3fV limit=%.3fV | "
            "ALERT pin was a transient spike. SW latch released. "
            "Voltage disagreement check suppressed for %ds. "
            "Field control returning to normal path.",
            busV, VoltageHardwareLimit,
            INA_OV_DISAGREE_SUPPRESS_MS / 1000);
        } else {
          // Still set — not a transient. Wait for 10s.
          queueConsoleMessageF(
            "INA228 OV STILL ACTIVE [3s check]: busV=%.3fV limit=%.3fV | "
            "BUSOL bit still set after clear attempt. Sustained condition. "
            "Will recheck at 10s.",
            busV, VoltageHardwareLimit);
        }
      }

      // ── 10s definitive check ─────────────────────────────────────────
      if (inaOvervoltageLatched && latchAge >= 10000) {
        earlyCheckDone = false;
        clearINA228AlertLatch(INA.getAddress());
        alertStatus = readINA228AlertRegister(INA.getAddress());
        float busV = INA.getBusVoltage();

        // I²C error (0xFFFF) → treat as "still set", hold latch (fail-safe)
        if (alertStatus != 0xFFFF && !(alertStatus & 0x0010)) {
          inaOvervoltageLatched = false;
          inaOvervoltageClearedMs = millis();
          queueConsoleMessageF(
            "INA228 OV CLEARED [10s timeout]: busV=%.3fV limit=%.3fV | "
            "BUSOL bit cleared. SW latch released. "
            "Voltage disagreement check suppressed for %ds. "
            "Field control returning to normal path.",
            busV, VoltageHardwareLimit,
            INA_OV_DISAGREE_SUPPRESS_MS / 1000);
        } else {
          // Still present — hold latch, will recheck on next 10s boundary
          // by resetting inaOvervoltageTime so the 10s check re-arms.
          inaOvervoltageTime = millis();
          currentAlarmCondition = true;
          alarmReason = "INA228 hardware overvoltage still present";
          queueConsoleMessageF(
            "INA228 OV SUSTAINED [10s check]: busV=%.3fV limit=%.3fV | "
            "BUSOL bit still set. Latch held. Will recheck in 10s.",
            busV, VoltageHardwareLimit);
        }
      }
    }

    // ── New event detection — throttled to every 5s ───────────────────
    // Hardware ALERT pin already cut GPIO4 before this runs.
    // This block detects the event and engages the software latch
    // so the field stays suppressed even if ALERT deasserts quickly.
    if (millis() - lastINA228Check >= 5000) {
      lastINA228Check = millis();
      alertStatus = readINA228AlertRegister(INA.getAddress());

      // I²C error (0xFFFF) → skip detection this tick (avoid false-positive OV latch)
      if (alertStatus != 0xFFFF && (alertStatus & 0x0010)) {
        if (!inaOvervoltageLatched) {
          inaOvervoltageLatched = true;
          inaOvervoltageTime = millis();
          currentAlarmCondition = true;

          float busV = INA.getBusVoltage();
          alarmReason = "INA228 hardware overvoltage";

          // Note: busV here may be lower than the actual peak because
          // the INA228 averaged value lags the instantaneous reading
          // that triggered the ALERT pin. The hardware caught the real peak;
          // this is a post-event averaged reading.
          queueConsoleMessageF(
            "INA228 HARDWARE ALERT PIN FIRED: busV(avg)=%.3fV limit=%.3fV | "
            "GPIO4 was cut electrically by ALERT pin before this message. "
            "SLOW_ALERT active: trigger used ~1054ms averaged value (128 samples). "
            "SW latch engaged. Disagreement check suppressed for %ds.",
            busV, VoltageHardwareLimit,
            INA_OV_DISAGREE_SUPPRESS_MS / 1000);
        }
      }
    }
  }

  // ========== MANUAL LATCH RESET ==========
  if (ResetAlarmLatch == 1) {
    alarmLatch = false;
    ResetAlarmLatch = 0;
    queueConsoleMessage("ALARM LATCH: Manually reset");
  }

  // ========== LATCHING LOGIC ==========
  if (AlarmLatchEnabled == 1) {
    if (currentAlarmCondition) {
      alarmLatch = true;
    }
    outputAlarmState = alarmLatch;
  } else {
    outputAlarmState = currentAlarmCondition;
  }

  // ========== FINAL OUTPUT CONTROL ==========
  bool finalOutput = false;
  if (AlarmTest == 1) {
    finalOutput = true;  // Force HIGH regardless of alarm state — test drives output directly
  } else if (AlarmActivate == 1) {
    finalOutput = outputAlarmState;
  }

  digitalWrite(21, finalOutput ? HIGH : LOW);
  alarmOutputState = finalOutput;           // Keep shared state in sync
  Alarm_Status = alarmOutputState ? 1 : 0;  // Set after finalOutput is computed

  // ========== CONSOLE MESSAGING ==========
  if (currentAlarmCondition != previousAlarmState) {
    if (currentAlarmCondition) {
      queueConsoleMessageF("ALARM ACTIVATED: %s", alarmReason);
    } else if (AlarmLatchEnabled == 0) {
      queueConsoleMessage("ALARM CLEARED");
    }
    previousAlarmState = currentAlarmCondition;
  }
}


// Human-readable boot/reset cause for the console boot line. esp_reset_reason()
// is constant for the whole session, so it can be read lazily when the line is emitted.
const char *resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_PANIC:     return "panic/crash";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

void logDashboardValues() {
  static unsigned long lastDashboardLog = 0;
  if (millis() - lastDashboardLog >= 300000) {  // Every 5 min — periodic time/context anchor
    lastDashboardLog = millis();
    queueConsoleMessageF("DASHBOARD: IBV=%.2fV SoC=%d%% AltI=%.1fA BattI=%.1fA AltT=%d°F RPM=%d",
                         IBV, SOC_percent / 100, MeasuredAmps, Bcur,
                         (int)AlternatorTemperatureF, (int)RPM);
  }
}

void applySocGainCorrection() {
  // Only apply if feature is enabled — battery current always comes from INA228
  if (AutoShuntGainCorrection == 0) {
    return;
  }

  // Check minimum time interval
  unsigned long now = millis();
  if (now - lastGainCorrectionTime < MIN_GAIN_CORRECTION_INTERVAL) {
    queueConsoleMessage("SOC Gain: Correction blocked, too soon since last adjustment");
    return;
  }

  // Calculate actual vs expected capacity
  float expectedCapacity = BatteryCapacity_Ah;
  float calculatedCapacity = CoulombCount_Ah_scaled / 100.0;  // Convert from scaled value

  // Sanity check - make sure we have reasonable values
  if (calculatedCapacity < 10 || expectedCapacity < 10) {
    queueConsoleMessage("SOC Gain: Invalid capacity values, skipping correction");
    return;
  }

  // Calculate error ratio
  float errorRatio = abs(expectedCapacity - calculatedCapacity) / expectedCapacity;

  // Check if error is too large to be reasonable
  if (errorRatio > MAX_REASONABLE_ERROR) {
    queueConsoleMessageF("SOC Gain: Error too large (%.1f%%), ignoring correction",
                         errorRatio * 100);
    return;
  }

  // Calculate desired correction factor
  float desiredCorrectionFactor = expectedCapacity / calculatedCapacity;
  float currentFactor = DynamicShuntGainFactor;
  float newFactor = currentFactor * desiredCorrectionFactor;

  // Limit the adjustment rate per cycle
  float maxChange = currentFactor * MAX_GAIN_ADJUSTMENT_PER_CYCLE;
  if (newFactor > currentFactor + maxChange) {
    newFactor = currentFactor + maxChange;
    queueConsoleMessage("SOC Gain: Correction limited to maximum change rate");
  } else if (newFactor < currentFactor - maxChange) {
    newFactor = currentFactor - maxChange;
    queueConsoleMessage("SOC Gain: Correction limited to maximum change rate");
  }

  // Apply bounds checking
  if (newFactor > MAX_DYNAMIC_GAIN_FACTOR) {
    newFactor = MAX_DYNAMIC_GAIN_FACTOR;
    queueConsoleMessageF("SOC Gain: Factor hit maximum limit (%.2f), check system",
                         MAX_DYNAMIC_GAIN_FACTOR);
  } else if (newFactor < MIN_DYNAMIC_GAIN_FACTOR) {
    newFactor = MIN_DYNAMIC_GAIN_FACTOR;
    queueConsoleMessageF("SOC Gain: Factor hit minimum limit (%.2f), check system",
                         MIN_DYNAMIC_GAIN_FACTOR);
  }

  // Apply the correction
  DynamicShuntGainFactor = newFactor;
  lastGainCorrectionTime = now;
  queueConsoleMessageF("SOC Gain: Corrected from %.4f to %.4f (Calc:%.1fAh, Expected:%.1fAh)",
                       currentFactor, newFactor, calculatedCapacity, expectedCapacity);
}
void handleSocGainReset() {
  if (ResetDynamicShuntGain == 1) {
    DynamicShuntGainFactor = 1.0;
    lastGainCorrectionTime = 0;
    ResetDynamicShuntGain = 0;  // Clear the momentary flag
    queueConsoleMessage("SOC Gain: Dynamic gain factor reset to 1.0");
  }
}
void checkAutoZeroTriggers() {
  // Only check if feature is enabled
  if (AutoAltCurrentZero == 0) {
    return;
  }

  // Don't start if already in progress
  if (autoZeroStartTime > 0) {
    return;
  }

  // Make sure engine is running
  if (RPM < 200) {
    return;
  }

  unsigned long now = millis();
  bool shouldTrigger = false;
  static char triggerReasonBuf[64];  // Static buffer for reason string
  const char *triggerReason = "";

  // Check time-based trigger (1 hour)
  if (now - lastAutoZeroTime >= AUTO_ZERO_INTERVAL) {
    shouldTrigger = true;
    triggerReason = "scheduled interval";
  }

  // Check temperature-based trigger (20°F change)
  float currentTemp = (TempSource == 0) ? AlternatorTemperatureF : temperatureThermistor;
  if (lastAutoZeroTemp > -900 && abs(currentTemp - lastAutoZeroTemp) >= AUTO_ZERO_TEMP_DELTA) {
    shouldTrigger = true;
    snprintf(triggerReasonBuf, sizeof(triggerReasonBuf), "temperature change (%.1f°F)",
             abs(currentTemp - lastAutoZeroTemp));
    triggerReason = triggerReasonBuf;
  }

  if (shouldTrigger) {
    startAutoZero(triggerReason);
  }
}
void startAutoZero(const char *reason) {  // Changed from String reason
  autoZeroStartTime = millis();
  queueConsoleMessageF("Auto-zero: Starting alternator current zeroing (%s)", reason);
}
void processAutoZero() {
  // Cancel if feature gets disabled during active cycle
  if (AutoAltCurrentZero == 0) {
    if (autoZeroStartTime > 0) {
      autoZeroStartTime = 0;      // Cancel active cycle
      autoZeroAccumulator = 0.0;  // Reset accumulator
      autoZeroSampleCount = 0;
      queueConsoleMessage("Auto-zero: Cancelled - feature disabled");
    }
    return;
  }

  // Only process if auto-zero is active
  if (autoZeroStartTime == 0) {
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - autoZeroStartTime;

  if (elapsed < AUTO_ZERO_DURATION) {
    // Still in zeroing phase - force field to minimum
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);
    digitalWrite(4, 0);  // Disable field completely for more accurate zero

    // Accumulate readings after 2 second settling time
    if (elapsed > 2000) {
      autoZeroAccumulator += MeasuredAmps;
      autoZeroSampleCount++;
    }
  } else {
    // Zeroing complete - calculate average and restore normal operation
    float currentTemp = (TempSource == 0) ? AlternatorTemperatureF : temperatureThermistor;

    // Calculate average of accumulated samples
    if (autoZeroSampleCount > 0) {
      DynamicAltCurrentZero = autoZeroAccumulator / autoZeroSampleCount;
    } else {
      DynamicAltCurrentZero = MeasuredAmps;  // Fallback to single reading
    }

    lastAutoZeroTime = now;
    lastAutoZeroTemp = currentTemp;
    autoZeroStartTime = 0;      // Clear active flag
    autoZeroAccumulator = 0.0;  // Reset accumulator
    autoZeroSampleCount = 0;
    queueConsoleMessageF("Auto-zero: Complete, new zero offset: %.3fA (avg of %d samples)",
                         DynamicAltCurrentZero, autoZeroSampleCount);
  }
}
void handleAltZeroReset() {
  if (ResetDynamicAltZero == 1) {
    DynamicAltCurrentZero = 0.0;
    lastAutoZeroTime = 0;
    lastAutoZeroTemp = -999.0;
    autoZeroStartTime = 0;    // Cancel any active auto-zero
    ResetDynamicAltZero = 0;  // Clear the momentary flag
    queueConsoleMessage("Auto-zero: Dynamic alternator zero offset reset to 0.0");
  }
}
void calculateChargeTimes() {
  static unsigned long lastCalcTime = 0;
  unsigned long now = millis();
  if (now - lastCalcTime < INA_SLOW_INTERVAL_MS) return;  // Only run every X seconds
  lastCalcTime = now;

  // Get the current amperage from INA228 battery shunt
  float currentAmps = getBatteryCurrent();  // this is for battery state

  if (currentAmps > 0.01) {  // charging
    // Calculate remaining capacity needed to reach 100%
    float currentSoC = SOC_percent / 100.0;  // Convert from scaled format
    float remainingCapacity = BatteryCapacity_Ah * (100.0 - currentSoC) / 100.0;
    timeToFullChargeMin = (int)(remainingCapacity / currentAmps * 60.0);
    timeToFullDischargeMin = -999;           // Not applicable while charging
  } else if (currentAmps < -0.01) {          // discharging
    float currentSoC = SOC_percent / 100.0;  // Convert from scaled format
    float availableCapacity = BatteryCapacity_Ah * currentSoC / 100.0;
    timeToFullDischargeMin = (int)(availableCapacity / (-currentAmps) * 60.0);
    timeToFullChargeMin = -999;  // Not applicable while discharging
  } else {
    // No significant current flow
    timeToFullChargeMin = -999;
    timeToFullDischargeMin = -999;
  }
}
void calculateThermalStress() {
  unsigned long now = millis();

  // **FIX 1: Initialize lastThermalUpdateTime on first run**
  if (lastThermalUpdateTime == 0) {
    lastThermalUpdateTime = now;
    return;  // Skip first calculation
  }

  if (now - lastThermalUpdateTime < THERMAL_UPDATE_INTERVAL) {
    return;
  }

  // **FIX 2: Add RPM validation**
  if (RPM < 0.0f || RPM > 10000.0f || isnan(RPM) || isinf(RPM)) {
    return;  // Invalid RPM
  }


  if (now - lastThermalUpdateTime < THERMAL_UPDATE_INTERVAL) {
    return;  // Not time to update yet
  }

  // **PROTECTION 0: Skip if temperature is invalid (NaN/Inf)**
  if (isnan(TempToUse) || isinf(TempToUse)) {
    return;  // Temperature sensor not initialized yet
  }

  // **PROTECTION 1: Validate temperature before using it (DS18B20 range: -67°F to +257°F)**
  if (TempToUse < -67.0f || TempToUse > 257.0f) {
    return;  // Skip calculation if temperature is outside sensor's valid range
  }

  float elapsedSeconds = (now - lastThermalUpdateTime) / 1000.0f;
  lastThermalUpdateTime = now;

  // Calculate component temperatures
  float T_winding_F = TempToUse + WindingTempOffset;
  float T_bearing_F = TempToUse + WindingTempOffset;  // temporarily using same offset for simplicity until we see how this thing works
  float T_brush_F = TempToUse + WindingTempOffset;    // temporarily using same offset for simplicity until we see how this thing works

  // **PROTECTION 2: Sanity check component temperatures (allow some headroom for offset)**
  if (T_winding_F < -100.0f || T_winding_F > 400.0f || T_bearing_F < -100.0f || T_bearing_F > 400.0f || T_brush_F < -100.0f || T_brush_F > 400.0f) {
    return;  // Don't accumulate damage from obviously bad readings
  }

  // Calculate alternator RPM
  float Alt_RPM = RPM * PulleyRatio;

  // Calculate individual component lives
  float T_winding_K = (T_winding_F - 32.0f) * 5.0f / 9.0f + 273.15f;
  float L_insul = L_REF_INSUL * exp(EA_INSULATION / BOLTZMANN_K * (1.0f / T_winding_K - 1.0f / T_REF_K));
  L_insul = min(L_insul, 10000.0f);

  float L_grease_base = L_REF_GREASE * pow(0.5f, (T_bearing_F - 158.0f) / 18.0f);
  float L_grease = L_grease_base * (6000.0f / max(Alt_RPM, 100.0f));
  L_grease = min(L_grease, 10000.0f);

  float temp_factor = 1.0f + 0.0025f * (T_brush_F - 150.0f);
  float L_brush = (L_REF_BRUSH * 6000.0f / max(Alt_RPM, 100.0f)) / max(temp_factor, 0.1f);
  L_brush = min(L_brush, 10000.0f);

  // Calculate damage rates (damage per hour)
  float insul_damage_rate = 1.0f / L_insul;
  float grease_damage_rate = 1.0f / L_grease;
  float brush_damage_rate = 1.0f / L_brush;

  // Accumulate damage over elapsed time
  float hours_elapsed = elapsedSeconds / 3600.0f;
  CumulativeInsulationDamage += insul_damage_rate * hours_elapsed;
  CumulativeGreaseDamage += grease_damage_rate * hours_elapsed;
  CumulativeBrushDamage += brush_damage_rate * hours_elapsed;

  // Constrain damage to 0-1 range
  CumulativeInsulationDamage = constrain(CumulativeInsulationDamage, 0.0f, 1.0f);
  CumulativeGreaseDamage = constrain(CumulativeGreaseDamage, 0.0f, 1.0f);
  CumulativeBrushDamage = constrain(CumulativeBrushDamage, 0.0f, 1.0f);

  // Calculate remaining life percentages
  InsulationLifePercent = (1.0f - CumulativeInsulationDamage) * 100.0f;
  GreaseLifePercent = (1.0f - CumulativeGreaseDamage) * 100.0f;
  BrushLifePercent = (1.0f - CumulativeBrushDamage) * 100.0f;

  // Calculate predicted life hours (minimum of current rates)
  float min_damage_rate = max({ insul_damage_rate, grease_damage_rate, brush_damage_rate });
  PredictedLifeHours = 1.0f / min_damage_rate;

  // Set indicator color
  if (PredictedLifeHours > 5000.0f) {
    LifeIndicatorColor = 0;  // Green
  } else if (PredictedLifeHours > 1000.0f) {
    LifeIndicatorColor = 1;  // Yellow
  } else {
    LifeIndicatorColor = 2;  // Red
  }
}


//INA228 functions to compensate for lack of library features related to ALERT pin
uint16_t readINA228AlertRegister(uint8_t i2cAddress) {
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x0B);                                     // DIAG_ALRT (correct register)
  if (Wire.endTransmission(false) != 0) return 0xFFFF;  // keep repeated start
  if (Wire.requestFrom(i2cAddress, (uint8_t)2) != 2) return 0xFFFF;
  return (Wire.read() << 8) | Wire.read();
}
bool clearINA228AlertLatch(uint8_t i2cAddress) {
  // Read CONFIG
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x00);  // CONFIG register
  Wire.endTransmission(false);
  Wire.requestFrom(i2cAddress, (uint8_t)2);
  if (Wire.available() < 2) return false;
  uint16_t config = (Wire.read() << 8) | Wire.read();
  // Set ALERT_LATCH_CLEAR bit (bit 3)
  config |= 0x0008;
  // Write CONFIG back
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x00);
  Wire.write(config >> 8);
  Wire.write(config & 0xFF);
  return Wire.endTransmission() == 0;
}

void updateINA228OvervoltageThreshold() {
  // Only update if INA228 is connected
  if (INADisconnected != 0) {
    queueConsoleMessageF("INA228: Cannot update threshold - chip not connected");
    return;
  }

  // Update the hardware limit based on current BulkVoltage setting
  VoltageHardwareLimit = BulkVoltage + 0.3;

  // Calculate threshold in LSB units for INA228 with proper rounding
  const double LSB = 0.003125;                                           // 3.125 mV/LSB
  uint16_t thresholdLSB = (uint16_t)(VoltageHardwareLimit / LSB + 0.5);  // Round instead of truncate

  // Diagnostic messages (no String churn)
  queueConsoleMessageF("INA228 threshold calc: %.3fV / %.6f = %u LSB", VoltageHardwareLimit, LSB, (unsigned)thresholdLSB);
  queueConsoleMessageF("INA228 effective threshold: %.3fV", (double)thresholdLSB * LSB);

  // Program overvoltage threshold and clear under-voltage
  INA.setBusOvervoltageTH(thresholdLSB);
  INA.setBusUndervoltageTH(0x0000);  // Clear under-voltage threshold (fix accidental setting)

  // Configure DIAG_ALRT behavior explicitly for predictable operation
  INA.setDiagnoseAlertBit(INA228_DIAG_SLOW_ALERT);        // Compare on SLOW\_ALERT uses the averaged value
  INA.clearDiagnoseAlertBit(INA228_DIAG_ALERT_LATCH);     // Transparent mode - alerts clear when condition clears
  INA.clearDiagnoseAlertBit(INA228_DIAG_ALERT_POLARITY);  // Active-low open-drain (default)
  INA.setDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT);    // Enable BUSOL reporting

  // Verify what was actually written to the chip
  uint16_t readback_BOVL = INA.getBusOvervoltageTH();
  uint16_t readback_BUVL = INA.getBusUndervoltageTH();

  queueConsoleMessageF("INA228 readback: BOVL=0x%04X (%.3fV), BUVL=0x%04X",
                       (unsigned)readback_BOVL, (double)readback_BOVL * LSB, (unsigned)readback_BUVL);

  // Verify writes were successful
  if (readback_BOVL != thresholdLSB) {
    queueConsoleMessageF("WARNING: BOVL write failed - expected 0x%04X, got 0x%04X",
                         (unsigned)thresholdLSB, (unsigned)readback_BOVL);
  }
  if (readback_BUVL != 0) {
    queueConsoleMessageF("WARNING: BUVL not cleared - still 0x%04X", (unsigned)readback_BUVL);
  }

  queueConsoleMessageF(
    "INA228: Overvoltage threshold=%.2fV | SLOW_ALERT ON (128-sample avg ~1054ms filter)",
    VoltageHardwareLimit);
}

void checkWebFilesExist() {
  const char *criticalFiles[] = {
    "/index.html.gz",
    "/styles.css.gz",
    "/script.js.gz",
    "/uPlot.min.css.gz",
    "/uPlot.iife.min.js.gz"
  };

  int missingCount = 0;
  Serial.println("=== CHECKING WEB FILES ===");

  // First ensure web filesystem is mounted
  if (!ensureWebFS()) {
    Serial.println("ERROR: Web filesystem not mounted!");
    queueConsoleMessage("CRITICAL: Web filesystem mount failed!");
    return;
  }

  for (int i = 0; i < 5; i++) {
    if (!webFS.exists(criticalFiles[i])) {
      Serial.printf("MISSING: %s\n", criticalFiles[i]);
      missingCount++;
    } else {
      Serial.printf("Found: %s\n", criticalFiles[i]);
    }
  }

  if (missingCount > 0) {
    Serial.println("===============================================");
    Serial.println("ERROR: Missing web interface files!");
    Serial.printf("Missing %d critical files from web partition\n", missingCount);
    Serial.println("Web files should be in factory_fs or prod_fs partition");
    Serial.println("Currently using: " + String(usingFactoryWebFiles ? "factory_fs" : "prod_fs"));
    Serial.println("===============================================");

    queueConsoleMessage("CRITICAL: " + String(missingCount) + " web files missing from web partition!");

  } else {
    Serial.println("All critical web files found");
    Serial.println("Using web files from: " + String(usingFactoryWebFiles ? "factory_fs" : "prod_fs"));
    queueConsoleMessage("Web interface files verified OK");
  }
}

// Inter-sample gap meter for a slow ADS channel (CH0 battV, CH2 RPM). Called on each
// VALID reading; records the gap since the previous valid reading. "worst" resets on
// Reset Peak Values. ch=0 → battV, ch=2 → RPM. Dashboard: Live Data → ESP32.
static inline void adsGapUpdate(uint8_t ch, uint32_t now) {
  uint32_t *prev  = (ch == 0) ? &ch0GapPrevMs  : &ch2GapPrevMs;
  uint16_t *last  = (ch == 0) ? &ch0GapLastMs  : &ch2GapLastMs;
  uint16_t *worst = (ch == 0) ? &ch0GapWorstMs : &ch2GapWorstMs;
  uint32_t g = (*prev == 0) ? 0 : (now - *prev);
  if (g > 65535u) g = 65535u;                  // clamp (a >65s gap means the channel is dead)
  *last = (uint16_t)g;
  if ((uint16_t)g > *worst) *worst = (uint16_t)g;
  *prev = now;
}


void ReadAnalogInputs() {
  // Outer wrapper — ft_rai_total captures the true worst-case duration including
  // I2C timeouts that the old INA228-only timer missed entirely. Individual
  // section timers triangulate which sub-block is responsible when ft_rai_total
  // spikes.
  TIMED_CALL(ft_rai_total, _ReadAnalogInputs_inner());
}

void _ReadAnalogInputs_inner() {

  // ── INA228 Battery Monitor ────────────────────────────────────────────────
  static unsigned long lastINARead_local = 0;

  // Speed mode switch — fast (4.3ms) when field gate is open, slow (1054ms) when off.
  // Writes two I2C registers only on transition; no cost on steady state.
  if (INADisconnected == 0) {
    bool fieldGateOpen = !gpio4IsLow;
    if (fieldGateOpen && !inaFastModeActive) {
      INA.setAverage(1);                    // 4 samples (register 1)
      INA.setBusVoltageConversionTime(4);   // 540µs
      INA.setShuntVoltageConversionTime(4); // 540µs — total update: 4×1080µs ≈ 4.3ms
      IBV_filtered = IBV;                   // reseed EMA so CV loop starts clean
      Bcur_filtered = Bcur;                 // §G: match IBV_filtered — reseed the CV battery-current PV on fast-mode entry
      inaReadInterval = INA_FAST_INTERVAL_MS;
      inaFastModeActive = true;
    } else if (!fieldGateOpen && inaFastModeActive) {
      INA.setAverage(4);                    // 128 samples (register 4)
      INA.setBusVoltageConversionTime(7);   // 4120µs
      INA.setShuntVoltageConversionTime(7); // 4120µs — total update: 128×8240µs ≈ 1054ms
      inaReadInterval = INA_SLOW_INTERVAL_MS;
      inaFastModeActive = false;
    }
  }

  if (millis() - lastINARead_local >= inaReadInterval) {
    if (INADisconnected == 0) {
      lastINARead_local = millis();
      // Track intervals only while INA228 is in fast mode (~5ms cadence).
      // Gating on inaFastModeActive (not fieldActiveStatus) avoids logging a
      // slow-mode ~1054ms read as a false outlier during gate transitions.
      // Reset windowed stats on each fast-mode rising edge.
      static bool lastFastMode = false;
      bool nowFastMode = inaFastModeActive;
      if (nowFastMode && !lastFastMode) resetINA228IntervalWindows();
      lastFastMode = nowFastMode;
      if (nowFastMode) recordINA228Interval(lastINARead_local);

      // ft_rai_ina228 / AnalogReadTime aliases: worstWindow + lastCall updated by macro
      TIMED_CALL(ft_rai_ina228, ([&]() {
                   try {
                     uint32_t inaBusT0 = micros();
                     IBV = INA.getBusVoltage();
                     ShuntVoltage_mV = INA.getShuntVoltage() * 1000;
                     uint32_t inaBusDt = micros() - inaBusT0;            // time in JUST the two Wire reads
                     if (inaBusDt > inaBusReadWorstUs) inaBusReadWorstUs = inaBusDt;
                     if (inaBusDt > 15000UL) inaBusSlowCount++;          // ≥1 Wire-timeout's worth = bus stall

                     // Sanity check the readings
                     if (!isnan(IBV) && IBV > 5.0 && IBV < 70.0 && !isnan(ShuntVoltage_mV)) {
                       Bcur = ShuntVoltage_mV * 1000.0f / ShuntResistanceMicroOhm;
                       Bcur = Bcur + BatteryCOffset;
                       // Apply inversion if needed
                       if (InvertBattAmps == 1) {
                         Bcur = -Bcur;
                       }
                       // Apply dynamic gain correction only when enabled — battery current always from INA228
                       if (AutoShuntGainCorrection == 1) {
                         Bcur = Bcur * DynamicShuntGainFactor;
                       }
                       BatteryCurrent_scaled = Bcur * 100;  // Store raw value for battery monitoring
                       // Only mark fresh on successful, valid readings
                       MARK_FRESH(IDX_IBV);
                       MARK_FRESH(IDX_BCUR);

                       // IBV EMA — used by getFiltV() and CV loop error terms
                       // dBcur/dt — positive value = load dump (loads disconnected, OV risk)
                       {
                         uint32_t nowIna = millis();
                         static bool ibv_ema_init = false;
                         static uint32_t lastIbvEmaMs = 0;
                         if (!ibv_ema_init) {
                           IBV_filtered = IBV;
                           ibv_ema_init = true;
                         } else {
                           float dt_f = fmaxf(1.0f, (float)(nowIna - lastIbvEmaMs));
                           float alpha = dt_f / (VoltageFilterTC + dt_f);
                           IBV_filtered = alpha * IBV + (1.0f - alpha) * IBV_filtered;
                         }
                         lastIbvEmaMs = nowIna;
                         ibvFreshFlag = true;

                         static float bcurPrev = 0.0f;
                         static uint32_t bcurPrevMs = 0;
                         if (bcurPrevMs > 0) {
                           uint32_t dtBcur = nowIna - bcurPrevMs;
                           if (dtBcur >= 3 && dtBcur < 2000) {
                             g_dBcur_dt = (Bcur - bcurPrev) / ((float)dtBcur * 0.001f);
                             rollUpdate(ROLL_LDSLEW, g_dBcur_dt);   // load-dump slew gate-tuning readout
                           }
                         }
                         bcurPrev = Bcur;
                         bcurPrevMs = nowIna;
                       }

                       // Bcur EMA — inner-PID process variable for CV battery-current control (§G).
                       // Mirrors g_pidI_filtered's EMA on MeasuredAmps, reusing OutputPIDFilterTC, but co-sampled
                       // with IBV at the fast-INA cadence (fresher than the loop fires). Raw Bcur above stays the
                       // load-dump dBcur/dt source; this filtered copy is only the PID feedback.
                       {
                         uint32_t nowBc = millis();
                         static bool bcur_ema_init = false;
                         static uint32_t lastBcurEmaMs = 0;
                         if (!bcur_ema_init) {
                           Bcur_filtered = Bcur;
                           bcur_ema_init = true;
                         } else {
                           float dt_f = fmaxf(1.0f, (float)(nowBc - lastBcurEmaMs));
                           float alpha = dt_f / (OutputPIDFilterTC + dt_f);
                           Bcur_filtered = alpha * Bcur + (1.0f - alpha) * Bcur_filtered;
                         }
                         lastBcurEmaMs = nowBc;
                       }

                       if (IBV > IBVMax)              { IBVMax              = IBV; }
                       if (IBV > PeakVoltage_AllTime) { PeakVoltage_AllTime = IBV; }
                       if (IBV < MinVoltage)          { MinVoltage          = IBV; }
                       if (IBV < MinVoltage_AllTime)  { MinVoltage_AllTime  = IBV; }
                       wmIgnUpdate(wmIgn_IBV,  IBV);   // ignition-cycle watermarks (lo + hi)
                       wmIgnUpdate(wmIgn_Bcur, Bcur);
                     } else {
                       ina228ErrorCount++;   // implausible read — dropped (no MARK_FRESH); was silent before
                     }

                   } catch (...) {
                     ina228ErrorCount++;   // exception path — visible on dashboard "I2C Bus Health"
                     // INA228 read failed - do not call MARK_FRESH. The counter replaces the old
                     // throttled console spam (now redundant); keep one rare Serial line for USB debug.
                     static unsigned long lastINAFailureWarning = 0;
                     if (millis() - lastINAFailureWarning > 10000) {
                       Serial.println("INA228 read failed");
                       lastINAFailureWarning = millis();
                     }
                   }
                 }()));
    }
  }

  // ── ADS1115 non-blocking state machine ───────────────────────────────────
  // Sequence {1,0,1,2,1,3} → CH1 = 3/6 samples (~213 Hz / ~4.7ms interval)
  // ADS_WAIT uses time-based 3ms delay — no isConversionDone() I²C poll.
  // Back-to-back trigger fires next conversion at end of ADS_READ_RESULT,
  // saving one loop() call per step. Falls back to ADS_IDLE if <2ms elapsed.
  // Full 6-step cycle ≈ 14ms; CH0/CH2/CH3 each update every ~14ms.
  //
  // ft_rai_ads_state measures cost per state step (not a full logical read cycle),
  // which is the correct unit for a non-blocking state machine.
  if (ADS1115Disconnected != 0) {
    // Throttled error message to prevent console spam
    static unsigned long lastADSWarning = 0;
    if (millis() - lastADSWarning > 10000) {  // Only warn every 10 seconds
      queueConsoleMessage("theADS1115 was not connected and triggered a return");
      lastADSWarning = millis();
    }
    return;  // Early exit - no MARK_FRESH calls
  }

  unsigned long now = millis();

  TIMED_CALL(ft_rai_ads_state, ([&]() {
               switch (adsState) {

                 case ADS_IDLE:
                   // Trigger single-shot conversion on current channel
                   adsTriggeredChannel = adsCurrentChannel;
                   adc.setMux(adsMuxCodes[adsTriggeredChannel]);
                   adc.triggerConversion();
                   adsStateEntered = millis();  // capture AFTER triggerConversion() write completes
                   adsState = ADS_WAIT;
                   break;

                 case ADS_WAIT:
                   // Time-based ready check — eliminates isConversionDone() I²C poll (requestFrom blindspot)
                   // 860 SPS = 1.16ms/conversion; 3ms gives millis() granularity margin
                   if (now - adsStateEntered >= ADS_CONVERSION_MS) {
                     adsState = ADS_READ_RESULT;
                   } else if (now - adsStateEntered > ADS_TIMEOUT_MS) {
                     queueConsoleMessage("ADS1115 timeout ch" + String(adsTriggeredChannel));
                     adsState = ADS_IDLE;  // retry same channel
                   }
                   break;

                 case ADS_READ_RESULT:
                   {
                     // Read conversion register directly - we have already confirmed OS bit is set,
                     // so we bypass adc.getConversion() which would busy-wait/block the loop unnecessarily
                     Wire.beginTransmission(0x48);
                     Wire.write(ADS1115_REG_POINTER_CONVERT);
                     uint32_t _ads_t0 = (uint32_t)esp_timer_get_time();
                     uint8_t endStatus = Wire.endTransmission(false);
                     uint32_t _ads_t1 = (uint32_t)esp_timer_get_time();
                     uint8_t bytesReceived = Wire.requestFrom((uint8_t)0x48, (uint8_t)2);
                     uint32_t _ads_t2 = (uint32_t)esp_timer_get_time();

                     // Diagnostic: log when either I2C call stalls >5ms total
                     if (_ads_t2 - _ads_t0 > 5000) {
                       adsSlowReadCount++;
                       adsLastSlowEndTxUs   = _ads_t1 - _ads_t0;
                       adsLastSlowReqFromUs = _ads_t2 - _ads_t1;
                       Serial.printf("ADS slow read #%lu ch%d: endTx=%luus reqFrom=%luus total=%luus\n",
                                     (unsigned long)adsSlowReadCount, adsTriggeredChannel,
                                     (unsigned long)adsLastSlowEndTxUs,
                                     (unsigned long)adsLastSlowReqFromUs,
                                     (unsigned long)(_ads_t2 - _ads_t0));
                     }

                     bool readOK = (endStatus == 0) && (bytesReceived == 2) && (Wire.available() >= 2);

                     if (readOK) {
                       Raw = (int16_t)((Wire.read() << 8) | Wire.read());
                     } else {
                       // Drain whatever is in the buffer to leave I2C bus clean
                       while (Wire.available()) Wire.read();
                       adsI2CErrorCount++;
                       queueConsoleMessage("ADS1115 I2C read error ch" + String(adsTriggeredChannel) + " endStatus=" + String(endStatus) + " bytes=" + String(bytesReceived));
                     }

                     // Track consecutive I²C failures — 5 in a row = chip gone mid-run
                     {
                       static uint8_t adsConsecFails = 0;
                       if (readOK) {
                         adsConsecFails = 0;
                       } else {
                         adsConsecFails++;
                         if (adsConsecFails >= 5) {
                           ADS1115Disconnected = 1;
                           queueConsoleMessage("ADS1115 declared disconnected after 5 consecutive I2C failures");
                           adsConsecFails = 0;
                         }
                       }
                     }

                     // CRITICAL: Use adsTriggeredChannel, NOT adsSequenceIndex!
                     // Only process and mark fresh on a confirmed good read
                     if (readOK) {
                       switch (adsTriggeredChannel) {
                         case 0:
                           Channel0V = Raw / 32768.0 * 6.144 * 21.0401;  // divider 1,000,000Ω / 49.9kΩ, scale ≈21.0401
                           BatteryV = Channel0V;
                           if (BatteryV > 5.0 && BatteryV < 70.0) {  // Sanity check
                             MARK_FRESH(IDX_BATTERY_V);              // Only mark fresh on valid reading
                             battVFreshFlag = true;
                             adsGapUpdate(0, now);  // CH0 battV inter-sample gap meter
                           }
                           break;

                           // case 1:
                           //   //Clamp On Sensor QNHCK1-21
                           //   Channel1V = Raw / 32768.0 * 6.144 * 2.0;  // divider is 768kΩ / 768kΩ, ratio = 0.5, scale = 2.000
                           //   MeasuredAmps = (Channel1V - 2.5) * 100;   // alternator current

                           //   if (InvertAltAmps == 1) {
                           //     MeasuredAmps = MeasuredAmps * -1;  // swap sign if necessary
                           //   }
                           //   MeasuredAmps = MeasuredAmps - AlternatorCOffset;
                           //   // Apply dynamic zero correction only when enabled
                           //   if (AutoAltCurrentZero == 1) {
                           //     MeasuredAmps = MeasuredAmps - DynamicAltCurrentZero;
                           //   }

                           //   if (MeasuredAmps > -500 && MeasuredAmps < 500) {  // Sanity check
                           //     MARK_FRESH(IDX_MEASURED_AMPS);
                           //     ch1FreshFlag = true;  // Signal PID that fresh current data is available
                           //   }


                         case 1:
                           ch1_record(millis());
                           //Clamp On Sensor QNHCK1-21
                           Channel1V = Raw / 32768.0 * 6.144 * 2.0;  // divider is 768kΩ / 768kΩ, ratio = 0.5, scale = 2.000
                           // Scale factor: sensor outputs 2.5V±2V over its rated range
                           // 0=±200A→100 A/V, 1=±300A→150 A/V, 2=±500A→250 A/V
                           {
                             static const float kAmpScale[]  = {100.0f, 150.0f, 250.0f};
                             static const float kSanityLim[] = {250.0f, 370.0f, 600.0f};
                             int rIdx = (AmpSensorRange >= 0 && AmpSensorRange <= 2) ? AmpSensorRange : 1;
                             MeasuredAmps = (Channel1V - 2.5f) * kAmpScale[rIdx];

                           if (InvertAltAmps == 1) {
                             MeasuredAmps = MeasuredAmps * -1;  // swap sign if necessary
                           }
                           MeasuredAmps = MeasuredAmps - AlternatorCOffset;
                           // Apply dynamic zero correction only when enabled
                           if (AutoAltCurrentZero == 1) {
                             MeasuredAmps = MeasuredAmps - DynamicAltCurrentZero;
                           }

                           if (MeasuredAmps > -kSanityLim[rIdx] && MeasuredAmps < kSanityLim[rIdx]) {  // Sanity check
                             MARK_FRESH(IDX_MEASURED_AMPS);
                             wmIgnUpdate(wmIgn_amps, MeasuredAmps);  // ignition-cycle watermark
                             ch1FreshFlag = true;  // Signal PID that fresh current data is available
                             // ── EMA filters ────────────────────────────────────────────────────────
                             // Display/log EMA (InputFilterTC → MeasuredAmps_filtered) and Output PID EMA
                             // (OutputPIDFilterTC → g_pidI_filtered) run independently so each can be tuned
                             // for its role. iExcess uses NEITHER — its own EMA reads raw MeasuredAmps into
                             // mExcessEma (see the MA block below). InputFilterTC is display/logging only now.
                             {
                               static bool amps_filter_init = false;
                               static uint32_t lastAmpsFilterMs = 0;
                               if (!amps_filter_init) {
                                 MeasuredAmps_filtered = MeasuredAmps;
                                 g_pidI_filtered       = MeasuredAmps;
                                 amps_filter_init = true;
                               } else {
                                 float dt_f = fmaxf(1.0f, (float)(now - lastAmpsFilterMs));
                                 float alpha_ie  = dt_f / (InputFilterTC      + dt_f);
                                 float alpha_pid = dt_f / (OutputPIDFilterTC  + dt_f);
                                 MeasuredAmps_filtered = alpha_ie  * MeasuredAmps + (1.0f - alpha_ie)  * MeasuredAmps_filtered;
                                 g_pidI_filtered       = alpha_pid * MeasuredAmps + (1.0f - alpha_pid) * g_pidI_filtered;
                               }
                               lastAmpsFilterMs = now;
                             }
                           }

                           // ── Current amplitude ring + moving averages ──────────────────────────────
                           // Runs every confirmed CH1 hit. Feeds MA and dI/dt to the fast current
                           // rise supervisor in AdjustFieldLearnMode.
                           {
                             uint32_t now_i = millis();
                             iAmpRing[iAmpHead] = { now_i, MeasuredAmps };
                             iAmpHead = (iAmpHead + 1) % I_RING_SIZE;
                             if (iAmpCount < I_RING_SIZE) iAmpCount++;

                             // MA(N) for the Output Current PID signal (iExcess no longer uses an MA —
                             // the EMA detector reads raw MeasuredAmps into its own mExcessEma).
                             {
                               int n_pid = OutputPIDMA_N < (int)iAmpCount ? OutputPIDMA_N : (int)iAmpCount;
                               if (n_pid < 1) n_pid = 1;
                               float sum_pid = 0.0f;
                               for (int k = 0; k < n_pid; k++) {
                                 uint8_t idx = (iAmpHead + I_RING_SIZE - 1 - k) % I_RING_SIZE;
                                 sum_pid += iAmpRing[idx].val;
                               }
                               g_pidMA_N = sum_pid / (float)n_pid;
                             }
                           }

                           // ── cvLog: write here, tied to actual CH1 sample arrival ──────────────────
                           // Removed from AdjustFieldLearnMode. Control-state globals (cv_I, Icv, etc.)
                           // reflect the previous control tick — one-tick lag is acceptable for analysis.
                           cvLog_tick(millis());

                           if (MeasuredAmps > MeasuredAmpsMax)         { MeasuredAmpsMax         = MeasuredAmps; }
                           if (MeasuredAmps > MeasuredAmpsMax_AllTime) { MeasuredAmpsMax_AllTime = MeasuredAmps; }
                           }  // end AmpSensorRange scale block
                           break;

                         case 2:
                           Channel2V = Raw / 32768.0 * 2 * 6.144 * RPMScalingFactor;
                           RPM = Channel2V;
                           // DEBUG — RPM-glitch hunt (the phantom "Engine STARTED RPM=3321" with a clean
                           // plot). A real start ramps; a lone jump from ~0 to a high value in one read is
                           // the glitch. Logs the raw ADS conversion code so we can read the actual bad
                           // value, plus transport status: endStatus=0/bytes=2 means a "clean" read of a
                           // bad conversion (incomplete or cross-channel mux carryover), not a bus error.
                           // Remove once root cause is identified.
                           {
                             static float prevRPMdbg = 0.0f;
                             if (prevRPMdbg < 100.0f && RPM > 1000.0f) {
                               queueConsoleMessageF("RPM GLITCH: RPM=%d rawCode=%d (0x%04X) scale=%d endStatus=%d bytes=%d mux=0x%04X",
                                                    (int)RPM, (int)Raw, (uint16_t)Raw, RPMScalingFactor,
                                                    (int)endStatus, (int)bytesReceived, (unsigned)adsMuxCodes[adsTriggeredChannel]);
                               Serial.printf("RPM GLITCH: RPM=%d rawCode=%d (0x%04X) scale=%d endStatus=%d bytes=%d mux=0x%04X\n",
                                             (int)RPM, (int)Raw, (uint16_t)Raw, RPMScalingFactor,
                                             (int)endStatus, (int)bytesReceived, (unsigned)adsMuxCodes[adsTriggeredChannel]);
                             }
                             prevRPMdbg = RPM;
                           }
                           if (RPM > RPMMax)         { RPMMax         = RPM; }
                           if (RPM > RPMMax_AllTime) { RPMMax_AllTime = RPM; }
                           if (RPM < 100) {
                             RPM = 0;
                           }
                           if (RPM >= 0 && RPM < 10000) {  // Sanity check
                             MARK_FRESH(IDX_RPM);          // Only mark fresh on valid reading
                             wmIgnUpdate(wmIgn_RPM, RPM);  // ignition-cycle watermark
                             adsGapUpdate(2, now);  // CH2 RPM inter-sample gap meter
                           }
                           break;

                         case 3:
                           // Channel3V = voltage at the ADC pin. ADS1115 gain ±6.144V FSR;
                           // real signal range is 0-3.3V per docs/hardware/analoginputsADS1115.md.
                           // Previous formula (* 833 * 2) was a leftover scaler from an old PCB rev
                           // and produced garbage values; the current scaling is plain volts at pin.
                           Channel3V = Raw / 32768.0 * 6.144;

                           // Disconnect detection: per the hardware doc, the lowest legitimate
                           // V_node is 0.134V (NTC at -40°C). The board ships with a ground jumper
                           // installed by default, so most units have Channel 3 tied to GND and
                           // V_node = 0V. A 0.05V floor cleanly distinguishes "no usable sensor"
                           // (grounded or floating-leakage) from any real reading.
                           if (Channel3V < 0.05f) {
                             temperatureThermistor = -99;
                             break;
                           }

                           temperatureThermistor = (int)(thermistorTempC(Channel3V) * 1.8f + 32.0f);  // °C → °F

                           if (temperatureThermistor > 500) {
                             temperatureThermistor = -99;
                           }
                           if (Channel3V > 0 && Channel3V < 3.3f) {  // legitimate ADC range
                             MARK_FRESH(IDX_CHANNEL3V);
                           }
                           if (temperatureThermistor > -58 && temperatureThermistor < 392) {  // °F bounds
                             MARK_FRESH(IDX_THERMISTOR_TEMP);

                             // Track max thermistor temperature
                             if (temperatureThermistor > MaxTemperatureThermistor)         MaxTemperatureThermistor         = temperatureThermistor;
                             if (temperatureThermistor > MaxTemperatureThermistor_AllTime) MaxTemperatureThermistor_AllTime = temperatureThermistor;
                           }
                           break;
                       }
                     }

                     // Advance to next channel and return to IDLE
                     // Change 1: sequence — CH1 gets 3 of 6 slots, worst-case gap = 2 conversion cycles
                     static const uint8_t adsSeq[] = { 1, 0, 1, 2, 1, 3 };  // was {0, 1, 0, 1, 2, 3}
                     static const uint8_t adsSeqLen = 6;

                     static uint8_t adsSeqIdx = 0;
                     adsSeqIdx = (adsSeqIdx + 1) % adsSeqLen;
                     adsCurrentChannel = adsSeq[adsSeqIdx];

                     // Back-to-back trigger: fire next conversion immediately if ≥2ms has
                     // elapsed since this conversion was triggered. At 860SPS (1.16ms) and
                     // ~3ms loop cadence this is always true, saving one loop() call per
                     // channel. Falls back to ADS_IDLE if called too soon (protects WiFi).
                     if (millis() - adsStateEntered >= 2) {
                       adsTriggeredChannel = adsCurrentChannel;
                       adc.setMux(adsMuxCodes[adsTriggeredChannel]);
                       adc.triggerConversion();
                       adsStateEntered = millis();
                       adsState = ADS_WAIT;
                     } else {
                       adsState = ADS_IDLE;
                     }
                     break;
                   }
               }
             }()));


  // ── BMP388 forced-mode non-blocking state machine ────────────────────────
  // x32 pressure, x2 temperature, IIR filter enabled
  // Trigger conversion, return immediately, poll until ready, burst-read once.
  // ft_rai_bmp_state measures cost per state step.
  {
    enum BMPState {
      BMP_IDLE,
      BMP_WAIT_READY
    };

    static BMPState bmpState = BMP_IDLE;
    static uint32_t bmpLastCycleMs = 0;
    static uint32_t bmpTriggerMs = 0;
    static bool bmpFirstReadDone = false;

    float temperature, pressure, altitude;

    TIMED_CALL(ft_rai_bmp_state, ([&]() {
                 switch (bmpState) {

                   case BMP_IDLE:
                     // 8s cycle keeps ambient/baro fresh well inside the 10s DATA_TIMEOUT window
                     if (millis() - bmpLastCycleMs >= 8000) {
                       bmp388.startForcedConversion();  // returns immediately if sensor is in sleep
                       bmpTriggerMs = millis();
                       bmpState = BMP_WAIT_READY;
                     }
                     break;

                   case BMP_WAIT_READY:
                     // Non-blocking poll. Returns 0 until data ready, then does the short read.
                     if (bmp388.getMeasurements(temperature, pressure, altitude)) {

                       float newPressure = pressure;  // already hPa / mbar in this library
                       float newTemp = temperature;  // keep in °C; conversion to °F happens at assignment below

                       if (!bmpFirstReadDone) {
                         bmpFirstReadDone = true;  // optional discard of first sample
                       } else {
                         if (isfinite(newPressure) && newPressure > 800.0f && newPressure < 1100.0f) {
                           baroPressure = newPressure;
                           MARK_FRESH(IDX_BARO_PRESSURE);
                         }

                         if (isfinite(newTemp) && newTemp > -40.0f && newTemp < 85.0f) {  // BMP388 rated range in °C
                           ambientTemp = newTemp * 1.8f + 32.0f;  // convert °C to °F for storage
                           MARK_FRESH(IDX_AMBIENT_TEMP);
                           // Board temp drifted → refresh the battery-temp gain derate. Gated to a ≥0.5°F move
                           // and only when a commissioned reference exists, so it's near-free. recomputeCvGains
                           // is already called cross-core from Core-0 web handlers; this task is also Core 0.
                           static float lastDerateTempF = NAN;
                           if (battTempDerateEnable && !isnan(CommissionTempF) &&
                               (isnan(lastDerateTempF) || fabsf(ambientTemp - lastDerateTempF) >= 0.5f)) {
                             lastDerateTempF = ambientTemp;
                             recomputeCvGains();
                           }
                         }
                       }

                       bmpLastCycleMs = millis();
                       bmpState = BMP_IDLE;
                     } else if (millis() - bmpTriggerMs > 120) {
                       // x32/x2 conversion should be done long before this; timeout just prevents getting stuck
                       static uint32_t lastBMPTimeoutMsg = 0;
                       if (millis() - lastBMPTimeoutMsg > 60000) {
                         Serial.println("BMP388 forced conversion timeout");
                         queueConsoleMessage("BMP388 forced conversion timeout");
                         lastBMPTimeoutMsg = millis();
                       }
                       bmpLastCycleMs = millis();
                       bmpState = BMP_IDLE;
                     }
                     break;
                 }
               }()));
  }

  // IMU FIFO drain runs in drainIMUFifo(), called from loop() after AdjustFieldLearnMode()
}

// ── IMU FIFO Drain ────────────────────────────────────────────────────────────
// Called from loop() AFTER AdjustFieldLearnMode() so a Wire stall on the IMU
// cannot delay control-critical voltage/current reads or the field control decision.
// No collision-avoidance needed here — a slow drain only delays telemetry, not control.
// Increment the shared I2C error counter and auto-disable the IMU if errors
// exceed 10 within any 60-second window.  Called from both error paths inside
// drainIMUFifo() so the threshold is shared across Get_FIFO_Num_Samples and
// Get_FIFO_Sample failures.
static void imuRecordI2CError() {
  static uint32_t windowErrors = 0;
  static unsigned long windowStart = 0;
  imu_i2c_error_count++;
  unsigned long now = millis();
  if (windowStart == 0 || now - windowStart >= 60000) {
    windowStart = now;
    windowErrors = 0;
  }
  windowErrors++;
  if (windowErrors >= 10 && imuEnabled) {
    imuEnabled = false;
    queueConsoleMessageF("IMU disabled: %u I2C errors in 60s (total %u). Check IMU wiring.", windowErrors, imu_i2c_error_count);
  }
}

void drainIMUFifo() {
  if (!imuEnabled || (millis() - lastIMUPoll < IMU_POLL_INTERVAL)) return;
  lastIMUPoll = millis();
  MARK_FRESH(IDX_IMU);

  TIMED_CALL(ft_rai_imu, ([&]() {
               uint16_t fifo_samples = 0;

               // Read FIFO status (quick operation, ~50 µs)
               if (imu.Get_FIFO_Num_Samples(&fifo_samples) != LSM6DSOX_OK) {
                 imuRecordI2CError();
                 return;
               }

               // Check for FIFO overrun
               uint8_t fifo_ovr = 0;
               if (imu.Get_FIFO_Overrun_Status(&fifo_ovr) == LSM6DSOX_OK && fifo_ovr) {
                 imu_fifo_overrun_count++;
                 static bool first_overrun = true;
                 static unsigned long last_overrun_msg = 0;
                 if (first_overrun) {
                   queueConsoleMessageF("IMU FIFO overrun detected (increase drain rate)");
                   first_overrun = false;
                   last_overrun_msg = millis();
                 } else if (millis() - last_overrun_msg > 300000) {
                   queueConsoleMessageF("IMU FIFO overruns: %u total", imu_fifo_overrun_count);
                   last_overrun_msg = millis();
                 }
               }

               if (fifo_samples == 0) return;

               uint16_t samples_to_read = (fifo_samples > MAX_FIFO_DRAIN_PER_POLL)
                                            ? MAX_FIFO_DRAIN_PER_POLL
                                            : fifo_samples;

               uint32_t imuFetchT0 = micros();
               if (imu.Get_FIFO_Sample(fifoBuffer, samples_to_read) != LSM6DSOX_OK) {
                 imuRecordI2CError();
                 return;
               }
               uint32_t imuFetchDt = micros() - imuFetchT0;   // time in JUST the FIFO Wire read
               if (imuFetchDt > imuFifoFetchWorstUs) {
                 imuFifoFetchWorstUs = imuFetchDt;
                 imuFifoWorstSamples = samples_to_read;   // bytes-in-flight at the worst fetch (×7 = bytes)
               }

               // Timestamp the batch end, then backdate each sample by its nominal interval.
               // The sensor's internal timestamp register (0x40-0x42, 25µs resolution) would give
               // true sample times but requires FIFO timestamp batching config. Using nominal ODR
               // intervals instead - error is negligible (<1% at these rates) and avoids the
               // batch_timestamp=same_value problem which caused dt_us=0 for all but the first
               // sample, effectively stalling the complementary filter.
               uint32_t batch_end_us = micros();
               constexpr uint32_t ACCEL_INTERVAL_US = 9615;  // 1,000,000 / 104 Hz
               constexpr uint32_t GYRO_INTERVAL_US = 19231;  // 1,000,000 / 52 Hz

               uint16_t accel_in_batch = 0, gyro_in_batch = 0;
               for (uint16_t i = 0; i < samples_to_read; i++) {
                 uint8_t tag = fifoBuffer[i * 7] >> 3;
                 if (tag == TAG_SENSOR_ACCEL) accel_in_batch++;
                 else if (tag == TAG_SENSOR_GYRO) gyro_in_batch++;
               }

               uint16_t accel_idx = 0, gyro_idx = 0;
               for (uint16_t i = 0; i < samples_to_read; i++) {
                 uint8_t raw_tag = fifoBuffer[i * 7];
                 uint8_t tag_sensor = raw_tag >> 3;

                 int16_t raw_x = (int16_t)((fifoBuffer[i * 7 + 2] << 8) | fifoBuffer[i * 7 + 1]);
                 int16_t raw_y = (int16_t)((fifoBuffer[i * 7 + 4] << 8) | fifoBuffer[i * 7 + 3]);
                 int16_t raw_z = (int16_t)((fifoBuffer[i * 7 + 6] << 8) | fifoBuffer[i * 7 + 5]);

                 if (tag_sensor == TAG_SENSOR_ACCEL) {
                   int16_t x = raw_x * ACCEL_X_SIGN;
                   int16_t y = raw_y * ACCEL_Y_SIGN;
                   int16_t z = raw_z * ACCEL_Z_SIGN;
                   uint32_t ts = batch_end_us - (uint32_t)(accel_in_batch - 1 - accel_idx) * ACCEL_INTERVAL_US;
                   pushAccelSample(x, y, z, ts);
                   accel_idx++;

                 } else if (tag_sensor == TAG_SENSOR_GYRO) {
                   int16_t x = raw_x * GYRO_X_SIGN;
                   int16_t y = raw_y * GYRO_Y_SIGN;
                   int16_t z = raw_z * GYRO_Z_SIGN;
                   uint32_t ts = batch_end_us - (uint32_t)(gyro_in_batch - 1 - gyro_idx) * GYRO_INTERVAL_US;
                   pushGyroSample(x, y, z, ts);
                   gyro_idx++;

                 } else if (tag_sensor == TAG_SENSOR_TEMP) {
                   // Temperature samples explicitly ignored — not batching temp in FIFO

                 } else {
                   imu_unknown_tag_count++;
                   static bool unknown_tag_warned = false;
                   if (!unknown_tag_warned) {
                     queueConsoleMessageF("IMU: unknown tag_sensor=%d (raw=0x%02X)", tag_sensor, raw_tag);
                     unknown_tag_warned = true;
                   }
                 }
               }
             }()));
}

void ReadAnalogInputs_Fake() {
  static unsigned long lastINARead_local2 = 0;

  if (millis() - lastINARead_local2 >= INA_SLOW_INTERVAL_MS) {  // could go down to 600 here, but this logic belongs in Loop anyway
    lastINARead_local2 = millis();                                 // ← ADD THIS LINE!

    static unsigned long lastFakeUpdate = 0;
    static float fakeVoltage = 13.2;
    static float fakeCurrent = -85.0;  // Start at -85A
    static float fakeRPM = 1000;
    static float fakeTemp = 45.0;

    // GPS motion simulation
    static float fakeLat = 0.0;     // Equator
    static float fakeLon = -140.0;  // Open Pacific
    static float fakeHeading = random(0, 360);
    static float fakeCOG = fakeHeading + random(-30, 30);
    static float fakeSOG = 3.0 + (random(-150, 150) / 100.0);  // 1.5–4.5 kt initial band
    // Wind simulation
    static float fakeApparentWindSpeed = 12.0;
    static float fakeApparentWindAngle = 45.0;
    fakeLon += 0.000375f;
    if (fakeLon > 180.0f) fakeLon -= 360.0f;
    // small lat oscillation near equator
    // 0.1 deg amplitude, period controlled by time
    // this uses fakeLon as the "time" driver, no new vars
    fakeLat = 0.1f * sin(fakeLon * 0.1f);

    LatitudeNMEA = fakeLat;
    LongitudeNMEA = fakeLon;
    SatelliteCountNMEA = 6 + random(0, 10);  // 6–15 sats
    MARK_FRESH(IDX_LATITUDE_NMEA);
    MARK_FRESH(IDX_LONGITUDE_NMEA);
    MARK_FRESH(IDX_SATELLITE_COUNT);

    // Speed Over Ground - vary a lot more
    fakeSOG += (random(-80, 80) / 100.0);  // ±0.8 kt per update
    if (fakeSOG < 0.5) fakeSOG = 0.5;
    if (fakeSOG > 12.0) fakeSOG = 12.0;
    SOGNMEA = fakeSOG;
    MARK_FRESH(IDX_SOG_NMEA);

    // Track max speed (session and lifetime)
    if (fakeSOG > MaxSpeed)         MaxSpeed         = fakeSOG;
    if (fakeSOG > MaxSpeed_AllTime) MaxSpeed_AllTime = fakeSOG;


    // Course Over Ground - wander more
    fakeCOG += (random(-40, 40) / 10.0);  // ±4° per update
    if (fakeCOG < 0) fakeCOG += 360;
    if (fakeCOG >= 360) fakeCOG -= 360;
    COGNMEA = fakeCOG;
    MARK_FRESH(IDX_COG_NMEA);

    // Heading - similar to COG but can differ more
    fakeHeading += (random(-60, 60) / 10.0);  // ±6° per update
    if (fakeHeading < 0) fakeHeading += 360;
    if (fakeHeading >= 360) fakeHeading -= 360;
    HeadingNMEA = fakeHeading;
    MARK_FRESH(IDX_HEADING_NMEA);

    // Apparent Wind
    fakeApparentWindSpeed += (random(-40, 40) / 10.0);  // ±4 kt per update
    if (fakeApparentWindSpeed < 0.0) fakeApparentWindSpeed = 0.0;
    if (fakeApparentWindSpeed > 35.0) fakeApparentWindSpeed = 35.0;
    ApparentWindSpeedNMEA = fakeApparentWindSpeed;
    MARK_FRESH(IDX_APPARENT_WIND_SPEED);

    fakeApparentWindAngle += (random(-120, 120) / 10.0);  // ±12° per update
    if (fakeApparentWindAngle < 0.0) fakeApparentWindAngle += 360.0;
    if (fakeApparentWindAngle >= 360.0) fakeApparentWindAngle -= 360.0;
    ApparentWindAngleNMEA = fakeApparentWindAngle;
    MARK_FRESH(IDX_APPARENT_WIND_ANGLE);

    // Generate fake battery voltage (11.5–15.0V range, much looser)
    fakeVoltage += (random(-80, 80) / 100.0);  // ±0.8 V per update
    if (fakeVoltage < 11.5) fakeVoltage = 11.5;
    if (fakeVoltage > 15.0) fakeVoltage = 15.0;
    BatteryV = fakeVoltage;
    IBV = fakeVoltage + (random(-30, 30) / 100.0);             // ±0.30 V
    VictronVoltage = fakeVoltage + (random(-50, 50) / 100.0);  // ±0.50 V
    MARK_FRESH(IDX_BATTERY_V);
    MARK_FRESH(IDX_IBV);
    MARK_FRESH(IDX_VICTRON_VOLTAGE);


    if (IBV > PeakVoltage_AllTime) { PeakVoltage_AllTime = IBV; }
    if (IBV > IBVMax)              { IBVMax              = IBV; }
    if (IBV < MinVoltage)          { MinVoltage          = IBV; }
    if (MinVoltage_AllTime == 0.0 || IBV < MinVoltage_AllTime) { MinVoltage_AllTime = IBV; }


    // Generate fake alternator current: wander heavily in a broad band
    //fakeCurrent += (random(-50, 50) / 100.0);  // ±0.5 A per update
    //if (fakeCurrent < -140.0) fakeCurrent = -140.0;
    // if (fakeCurrent > 150.0) fakeCurrent = 150.0;
    fakeCurrent = 10;
    MeasuredAmps = fakeCurrent * (InvertAltAmps ? -1 : 1);  // Apply invert flag
    ch1FreshFlag = true;                                    // Signal PID that fresh current data is available
    MARK_FRESH(IDX_MEASURED_AMPS);

    if (MeasuredAmps > MeasuredAmpsMax)         { MeasuredAmpsMax         = MeasuredAmps; }
    if (MeasuredAmps > MeasuredAmpsMax_AllTime) { MeasuredAmpsMax_AllTime = MeasuredAmps; }


    // Generate fake battery current - broad noisy range
    static float fakeBattCurrent = 0;
    if (SOC_percent > 99) {
      fakeBattCurrent = -100.0;
    }
    if (SOC_percent < 99) {
      fakeBattCurrent = 100.0;
    }
    //fakeBattCurrent += (random(-80, 80) / 10.0);  // ±8 A per update
    //if (fakeBattCurrent < -180.0) fakeBattCurrent = -180.0;
    //if (fakeBattCurrent > 180.0) fakeBattCurrent = 180.0;
    fakeBattCurrent = 100;
    Bcur = fakeBattCurrent * (InvertBattAmps ? -1 : 1);  // Apply invert flag
    Bcur_filtered = Bcur;                                 // fake mode: no EMA lag needed (§G CV battery-current PV)
    BatteryCurrent_scaled = Bcur * 100;
    VictronCurrent = Bcur + (random(-80, 80) / 10.0);  // ±8 A offset
    MARK_FRESH(IDX_BCUR);
    MARK_FRESH(IDX_VICTRON_CURRENT);

    // Generate fake RPM (800–3200 range, looser)
    //fakeRPM += (random(-3, 3));  // ±3 rpm per update
    // if (fakeRPM < 800) fakeRPM = 800;
    // if (fakeRPM > 3200) fakeRPM = 3200;
    fakeRPM = 1000;
    RPM = fakeRPM;
    MARK_FRESH(IDX_RPM);

    if (RPM > RPMMax)         { RPMMax         = RPM; }
    if (RPM > RPMMax_AllTime) { RPMMax_AllTime = RPM; }

    // Generate fake temperatures (10–110°C, looser swings)
    fakeTemp += (random(-30, 30) / 10.0);
    if (fakeTemp < 10) fakeTemp = 10;
    if (fakeTemp > 110) fakeTemp = 110;
    temperatureThermistor = fakeTemp;
    ambientTemp = fakeTemp - (5 + random(-20, 20) / 10.0);

    float tempC = fakeTemp + 10 + random(-20, 20) / 10.0;
    AlternatorTemperatureF = tempC * 9.0 / 5.0 + 32.0;
    MARK_FRESH(IDX_THERMISTOR_TEMP);
    MARK_FRESH(IDX_ALTERNATOR_TEMP);

    // Track temperature maxes
    if (temperatureThermistor > MaxTemperatureThermistor)             MaxTemperatureThermistor             = temperatureThermistor;
    if (temperatureThermistor > MaxTemperatureThermistor_AllTime)     MaxTemperatureThermistor_AllTime     = temperatureThermistor;
    if (AlternatorTemperatureF > MaxAlternatorTemperatureF)           MaxAlternatorTemperatureF            = AlternatorTemperatureF;
    if (AlternatorTemperatureF > MaxAlternatorTemperatureF_AllTime)   MaxAlternatorTemperatureF_AllTime    = AlternatorTemperatureF;


    // Solar energy simulation (fake Victron data)
    static unsigned long lastSolarUpdate = 0;
    if (millis() - lastSolarUpdate >= 2000) {  // Update every 2 seconds like real VE.Direct
      lastSolarUpdate = millis();

      // Simulate solar power output (100–800W range, quite loose)
      static float fakeSolarPower = 300.0;
      fakeSolarPower += (random(-300, 300));  // ±300 W per update
      if (fakeSolarPower < 100) fakeSolarPower = 100;
      if (fakeSolarPower > 800) fakeSolarPower = 800;

      // Calculate energy: Power × time = energy
      float elapsedSeconds = 2.0;  // 2 second update interval
      float solarEnergyDelta_Wh = (fakeSolarPower * elapsedSeconds) / 3600.0f;

      // Mirror the fake solar onto the live dashboard fields + leaderboard watermark
      VictronSolarPower_W = fakeSolarPower;
      VictronSolarVoltage_V = 18.0f + (random(-200, 200) / 100.0f);
      VictronSolarCurrent_A = (VictronSolarVoltage_V > 1.0f) ? (VictronSolarPower_W / VictronSolarVoltage_V) : 0.0f;
      VictronChargeState = 3; VictronMPPTMode = 2; VictronError = 0;   // Bulk / Active MPPT / OK
      VictronYieldToday_kWh += solarEnergyDelta_Wh / 1000.0f;
      if (fakeSolarPower > VictronMaxPowerToday_W) VictronMaxPowerToday_W = fakeSolarPower;
      MARK_FRESH(IDX_VICTRON_SOLAR);
      if (VictronSolarPower_W > solar_power_max_alltime_w) solar_power_max_alltime_w = VictronSolarPower_W;

      static float solarEnergyAccumulator = 0.0f;
      static float solarEnergyAccumulator_AllTime = 0.0f;

      solarEnergyAccumulator += solarEnergyDelta_Wh;
      solarEnergyAccumulator_AllTime += solarEnergyDelta_Wh;

      if (solarEnergyAccumulator >= 1.0f) {
        SolarChargedEnergy += (int)solarEnergyAccumulator;
        solarEnergyAccumulator -= (int)solarEnergyAccumulator;
      }

      if (solarEnergyAccumulator_AllTime >= 1.0f) {
        SolarChargedEnergy_AllTime += (int)solarEnergyAccumulator_AllTime;
        solarEnergyAccumulator_AllTime -= (int)solarEnergyAccumulator_AllTime;
      }
    }

    // Fake barometric pressure (wide, stormy)
    baroPressure = 1013 + (random(-200, 200) / 10.0);  // ±20 hPa

    // Fake other channels
    Channel0V = BatteryV;
    Channel1V = 2.5 + (MeasuredAmps / 50.0);  // more aggressive scaling
    Channel2V = RPM / RPMScalingFactor;
    Channel3V = 50 + (random(-60, 60));  //  -10–110 range
    MARK_FRESH(IDX_CHANNEL3V);
  }

  // ============================================================================
  // IMU FAKE DATA GENERATION
  // ============================================================================
  if (imuEnabled) {
    static float fakeHeelAngle = 0;
    static float fakePitchAngle = 0;
    static float fakeHeelRate = 0;
    static float fakePitchRate = 0;
    static unsigned long lastIMUFake = 0;

    unsigned long now = millis();
    float dt = (now - lastIMUFake) / 1000.0f;
    if (lastIMUFake == 0) dt = 0.1f;  // First call
    lastIMUFake = now;

    // Advance motion phases every call so heel/pitch track real time even when pushes are throttled
    static float rollPhase = 0;
    rollPhase += dt * 0.5f;                                              // ~0.5 rad/s = ~12s period
    fakeHeelAngle = 8.0f * sin(rollPhase) + (random(-30, 30) / 10.0f);  // ±8° ±3° noise
    fakeHeelRate = 8.0f * 0.5f * cos(rollPhase);                        // Derivative

    static float pitchPhase = 0;
    pitchPhase += dt * 0.3f;                                               // ~0.3 rad/s = ~20s period
    fakePitchAngle = 4.0f * sin(pitchPhase) + (random(-20, 20) / 10.0f);  // ±4° ±2° noise
    fakePitchRate = 4.0f * 0.3f * cos(pitchPhase);                        // Derivative

    // Rate-limit sample push to ~10 Hz — avoids flooding ring buffer at fast main loop rates
    static unsigned long lastIMUPush = 0;
    if (now - lastIMUPush < 100) return;
    lastIMUPush = now;

    // Slam event (~2% per push call at 10 Hz = ~1 per 5 seconds, rough conditions)
    // Amplitude kept within ±2g sensor range to avoid int16_t overflow
    static float slamDecay = 0;
    if (random(0, 100) < 2 && slamDecay == 0) {
      slamDecay = 0.6f + (random(0, 30) / 100.0f);  // 0.6–0.9g spike
    }
    if (slamDecay > 0) {
      slamDecay -= dt * 8.0f;
      if (slamDecay < 0) slamDecay = 0;
    }

    // Convert angles to accelerations (sensor frame)
    // When boat heels, gravity vector rotates
    float heel_rad = fakeHeelAngle * PI / 180.0f;
    float pitch_rad = fakePitchAngle * PI / 180.0f;

    // Gravity components in sensor frame (simplified, no yaw)
    float ax_g = -sin(pitch_rad);                 // Forward/aft tilt
    float ay_g = sin(heel_rad) * cos(pitch_rad);  // Port/starboard tilt
    float az_g = cos(heel_rad) * cos(pitch_rad);  // Vertical (1g when level)

    // Add vertical slam spike
    az_g += slamDecay;

    // Add wave-induced vertical motion (~0.3g peak, ~8s period)
    static float wavePhase = 0;
    wavePhase += dt * 0.8f;
    az_g += 0.3f * sin(wavePhase) + (random(-20, 20) / 100.0f);

    // Convert to raw sensor counts — constrain prevents int16_t overflow on spikes
    // ACCEL_SCALE = 0.000061 g/LSB → 1g = 16393 LSB, full range ±32767 = ±2.0g
    int16_t raw_ax = (int16_t)constrain((int32_t)(ax_g / ACCEL_SCALE), -32767, 32767);
    int16_t raw_ay = (int16_t)constrain((int32_t)(ay_g / ACCEL_SCALE), -32767, 32767);
    int16_t raw_az = (int16_t)constrain((int32_t)(az_g / ACCEL_SCALE), -32767, 32767);

    // Gyro rates (already in dps)
    // GYRO_SCALE = 0.070 dps/LSB → 1 dps = 14.3 LSB
    float gx_dps = fakePitchRate + (random(-50, 50) / 100.0f);  // ±0.5 dps noise
    float gy_dps = fakeHeelRate + (random(-50, 50) / 100.0f);
    float gz_dps = (random(-30, 30) / 100.0f);  // Small yaw rate noise

    int16_t raw_gx = (int16_t)(gx_dps / GYRO_SCALE);
    int16_t raw_gy = (int16_t)(gy_dps / GYRO_SCALE);
    int16_t raw_gz = (int16_t)(gz_dps / GYRO_SCALE);

    // Engine + sea-state vibration noise scaled with RPM
    // At 1500 RPM: ±2500 LSB ≈ ±150 mg per axis (≈90 mg RMS)
    // At idle/stopped: minimum 200 LSB keeps background sea-state noise present
    int engineNoise = (int)constrain(RPM / 0.6f, 200.0f, 6000.0f);

    // Push samples to ring buffers — simulates a batch from the real 104 Hz FIFO
    uint32_t timestamp_us = micros();

    // 5 accel samples at ~9.6ms spacing = 104Hz claimed rate
    for (int i = 0; i < 5; i++) {
      int16_t jitter_ax = (int16_t)constrain((int32_t)raw_ax + random(-engineNoise, engineNoise), -32767, 32767);
      int16_t jitter_ay = (int16_t)constrain((int32_t)raw_ay + random(-engineNoise, engineNoise), -32767, 32767);
      int16_t jitter_az = (int16_t)constrain((int32_t)raw_az + random(-engineNoise, engineNoise), -32767, 32767);
      pushAccelSample(jitter_ax, jitter_ay, jitter_az, timestamp_us + i * 9615);
    }

    // Gyro at 52 Hz (one sample per ~20ms)
    pushGyroSample(raw_gx, raw_gy, raw_gz, timestamp_us + 19000);
  }
}


bool validateWebFile(const char *filename) {
  esp_task_wdt_reset();  // Feed watchdog before file operations

  Serial.printf("\n=== VALIDATING WEB FILE: %s ===\n", filename);
  Serial.flush();

  File file = webFS.open(filename, "r");
  if (!file) {
    Serial.printf("ERROR: %s - FILE NOT FOUND\n", filename);
    Serial.flush();
    return false;
  }

  size_t fileSize = file.size();
  Serial.printf("File size: %d bytes\n", fileSize);
  Serial.flush();

  if (fileSize < 18) {  // Minimum valid gzip file size
    Serial.printf("ERROR: %s - FILE TOO SMALL (%d bytes, need at least 18)\n", filename, fileSize);
    Serial.flush();
    file.close();
    return false;
  }

  // Check gzip magic bytes (0x1F 0x8B)
  Serial.printf("Checking gzip magic bytes...\n");
  Serial.flush();
  uint8_t magic[2];
  size_t bytesRead = file.read(magic, 2);
  file.close();

  bool validGzip = (bytesRead == 2 && magic[0] == 0x1F && magic[1] == 0x8B);

  if (validGzip) {
    Serial.printf("SUCCESS: %s - Valid gzip file (magic bytes 0x1F 0x8B confirmed) ✓\n", filename);
  } else {
    Serial.printf("FAILED: %s - Invalid gzip magic bytes (got 0x%02X 0x%02X, expected 0x1F 0x8B) ✗\n",
                  filename, magic[0], magic[1]);
  }
  Serial.printf("=== VALIDATION COMPLETE: %s ===\n\n", filename);
  Serial.flush();

  esp_task_wdt_reset();  // Feed watchdog after file operations

  return validGzip;
}

bool validateWebFilesystem() {
  Serial.println("\n--- Validating all web files ---");
  Serial.flush();

  bool result = true;
  result = validateWebFile("/index.html.gz") && result;
  result = validateWebFile("/styles.css.gz") && result;
  result = validateWebFile("/script.js.gz") && result;
  result = validateWebFile("/uPlot.min.css.gz") && result;
  result = validateWebFile("/uPlot.iife.min.js.gz") && result;

  if (result) {
    Serial.println("--- All web files validated successfully ---");
  } else {
    Serial.println("--- One or more web files FAILED validation (see above) ---");
  }
  Serial.flush();

  return result;
}
bool ensureWebFS() {
  esp_task_wdt_reset();  // Feed watchdog at start

  static bool webMounted = false;
  if (webMounted) {
    Serial.println("Web filesystem already mounted - skipping");
    return true;
  }

  Serial.println("\n========================================");
  Serial.println("=== MOUNTING WEB FILESYSTEM ===");
  Serial.println("========================================");
  Serial.flush();

  // Check which partition we're running from
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

  bool runningFromFactory = (running_partition == factory_partition);

  Serial.printf("Running from: %s\n", runningFromFactory ? "FACTORY partition" : "OTA partition");
  Serial.flush();

  if (runningFromFactory) {
    Serial.println("Attempting to mount factory_fs...");
    Serial.flush();

    if (webFS.begin(true, "/web", 10, "factory_fs")) {
      Serial.println("factory_fs mounted successfully");
      Serial.flush();
      Serial.println("Starting validation of web files...");
      Serial.flush();

      if (validateWebFilesystem()) {
        usingFactoryWebFiles = true;
        Serial.println("\n✓✓✓ SUCCESS: factory_fs validated and mounted ✓✓✓");
        Serial.flush();
        webMounted = true;
        cacheGzFiles();  // <-- ADDED
        esp_task_wdt_reset();
        return true;
      } else {
        Serial.println("\n✗✗✗ CRITICAL ERROR: Factory web files FAILED validation ✗✗✗");
        Serial.println("This should never happen - factory partition is corrupted!");
        Serial.flush();
        delay(5000);
        return false;
      }
    } else {
      Serial.println("✗✗✗ CRITICAL ERROR: Failed to mount factory_fs ✗✗✗");
      Serial.flush();
      delay(5000);
      return false;
    }

  } else {
    Serial.println("Attempting to mount prod_fs (OTA web files)...");
    Serial.flush();

    if (webFS.begin(true, "/web", 10, "prod_fs")) {
      Serial.println("prod_fs mounted successfully");
      Serial.flush();
      Serial.println("Starting validation of OTA web files...");
      Serial.flush();

      if (validateWebFilesystem()) {
        usingFactoryWebFiles = false;
        Serial.println("\n✓✓✓ SUCCESS: prod_fs validated and mounted ✓✓✓");
        Serial.flush();
        webMounted = true;
        cacheGzFiles();  // <-- ADDED
        esp_task_wdt_reset();
        return true;

      } else {
        Serial.println("\n✗✗✗ VALIDATION FAILED: prod_fs is corrupted! ✗✗✗");
        Serial.println("OTA web files are corrupted or incomplete");
        Serial.println("Triggering factory rollback in 5 seconds...");
        Serial.println("You will see which file(s) failed validation above ^^^");
        Serial.flush();

        webFS.end();
        queueConsoleMessage("CRITICAL: prod_fs corrupted, forcing factory rollback");

        delay(5000);

        Serial.println("NOW REBOOTING TO FACTORY FIRMWARE...");
        Serial.flush();
        delay(1000);

        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
    }

    Serial.println("\n--- prod_fs mount failed, trying factory_fs fallback ---");
    Serial.flush();

    if (webFS.begin(true, "/web", 10, "factory_fs")) {
      Serial.println("factory_fs mounted successfully (fallback)");
      Serial.flush();
      Serial.println("Starting validation of factory web files (fallback)...");
      Serial.flush();

      if (validateWebFilesystem()) {
        usingFactoryWebFiles = true;
        Serial.println("\n✓✓✓ SUCCESS: factory_fs validated and mounted (fallback from prod_fs) ✓✓✓");
        Serial.flush();
        webMounted = true;
        cacheGzFiles();  // <-- ADDED
        esp_task_wdt_reset();
        return true;

      } else {
        Serial.println("\n✗✗✗ CATASTROPHIC ERROR: Both web filesystems are corrupted! ✗✗✗");
        Serial.println("prod_fs: FAILED");
        Serial.println("factory_fs: FAILED");
        Serial.println("System cannot continue - halting in 10 seconds");
        Serial.flush();
        webFS.end();
        delay(10000);
        return false;
      }
    }

    Serial.println("✗✗✗ ERROR: Could not mount any web filesystem ✗✗✗");
    Serial.flush();
    delay(5000);
    return false;
  }
}
void switchToFactoryWebFiles() {
  webFS.end();
  usingFactoryWebFiles = true;
  if (!webFS.begin(true, "/web", 10, "factory_fs")) {
    Serial.println("ERROR: Failed to mount factory web files");
    events.send("CRITICAL: Failed to mount factory web files", "console", millis());
  } else {
    Serial.println("Switched to factory web files");
    events.send("Switched to factory web files for recovery", "console", millis());
  }
}
void performFactoryReset() {
  Serial.println("FACTORY RESET: Switching to factory firmware and web files");
  events.send("FACTORY RESET: Switching to factory firmware and web files", "console", millis());

  const esp_partition_t *factory_partition =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

  if (factory_partition) {
    esp_ota_set_boot_partition(factory_partition);
    switchToFactoryWebFiles();
    Serial.println("Factory reset complete - restarting in 3 seconds");
    events.send("Factory reset complete - restarting in 3 seconds", "console", millis());
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("ERROR: Factory partition not found");
    events.send("ERROR: Factory partition not found", "console", millis());
  }
}

void calculateDerivedMetrics() {
  // ===== TRUE WIND CALCULATION =====
  // Skip if the bus already provided true wind directly (WindSpeed PGN handler populated
  // it from a True_North / Magnetic / True_boat reference). Only derive from apparent
  // when no upstream true source is available.
  if (!IS_STALE(IDX_APPARENT_WIND_SPEED) && !IS_STALE(IDX_APPARENT_WIND_ANGLE) &&
      (!IS_STALE(IDX_STW_NMEA) || !IS_STALE(IDX_SOG_NMEA)) && !IS_STALE(IDX_HEADING_NMEA) &&
      IS_STALE(IDX_TRUE_WIND_SPEED)) {

    // Convert apparent wind angle to radians for calculation
    float awaRad = ApparentWindAngleNMEA * PI / 180.0;

    // Calculate true wind speed and direction using vector mathematics
    // True wind = Apparent wind - Boat motion vector
    float awsX = ApparentWindSpeedNMEA * sin(awaRad);  // Apparent wind X component
    float awsY = ApparentWindSpeedNMEA * cos(awaRad);  // Apparent wind Y component

    // Subtract boat speed (boat speed is in direction of heading, so Y component).
    // Prefer speed-through-water (STW) — it removes current so true wind isn't contaminated;
    // fall back to SOG when no water-speed log is present.
    float boatSpeedTW = (!IS_STALE(IDX_STW_NMEA) && !isnan(STWNMEA)) ? STWNMEA : SOGNMEA;
    float twsX = awsX;
    float twsY = awsY - boatSpeedTW;

    // Calculate true wind speed and angle
    TrueWindSpeedNMEA = sqrt(twsX * twsX + twsY * twsY);
    TrueWindAngleNMEA = atan2(twsX, twsY) * 180.0 / PI;

    // Normalize angle to 0-360
    if (TrueWindAngleNMEA < 0) TrueWindAngleNMEA += 360.0;

    MARK_FRESH(IDX_TRUE_WIND_SPEED);
    MARK_FRESH(IDX_TRUE_WIND_ANGLE);
  } else if (IS_STALE(IDX_TRUE_WIND_SPEED) && IS_STALE(IDX_TRUE_WIND_ANGLE)) {
    // No upstream true PGN AND no apparent-derived path — clear to NAN.
    TrueWindSpeedNMEA = NAN;
    TrueWindAngleNMEA = NAN;
  }
  // (else: bus is providing true wind directly via WindSpeed PGN handler — leave it alone)

  // ===== LEEWAY CALCULATION =====
  // Only calculate if we have valid heading and COG
  if (!IS_STALE(IDX_HEADING_NMEA) && !IS_STALE(IDX_COG_NMEA)) {
    LeewayNMEA = HeadingNMEA - COGNMEA;

    // Normalize to -180 to +180 range
    if (LeewayNMEA > 180.0) LeewayNMEA -= 360.0;
    if (LeewayNMEA < -180.0) LeewayNMEA += 360.0;

    MARK_FRESH(IDX_LEEWAY);
  } else {
    LeewayNMEA = NAN;
  }

  // ===== VELOCITY MADE GOOD (two independent values, both always computed — no mode toggle) =====
  // VMGNMEA   = VMG toward the manual target bearing: SOG·cos(targetBearing − COG).
  // VMGUpwind = VMG to windward: SOG·cos(TWA). TrueWindAngleNMEA is boat-relative, so NO COG term.
  if (!IS_STALE(IDX_SOG_NMEA) && !IS_STALE(IDX_COG_NMEA) && VMGTargetBearing >= 0) {
    float bearingDiff = (VMGTargetBearing - COGNMEA) * PI / 180.0;
    VMGNMEA = SOGNMEA * cos(bearingDiff);
    MARK_FRESH(IDX_VMG);
  } else {
    VMGNMEA = NAN;  // no manual target set, or SOG/COG stale
  }
  if (!IS_STALE(IDX_SOG_NMEA) && !IS_STALE(IDX_TRUE_WIND_ANGLE)) {
    VMGUpwind = SOGNMEA * cos(TrueWindAngleNMEA * PI / 180.0);
  } else {
    VMGUpwind = NAN;  // need boat speed + true wind angle
  }
  // Session min/max watermarks for both VMGs (dashboard ↑/↓ sidebars)
  wmIgnUpdate(wmIgn_VMGman, VMGNMEA);
  wmIgnUpdate(wmIgn_VMGup, VMGUpwind);
  // Lifetime best upwind VMG — only while the engine is effectively off (sailing, RPM<50), so motoring can't pad the record.
  if (isfinite(VMGUpwind) && RPM < 50.0f && VMGUpwind > best_upwind_vmg_alltime) {
    best_upwind_vmg_alltime = VMGUpwind;
  }

  // ===== SUSTAINED TRUE WIND → BEAUFORT & GALE =====
  // 2-min running average of true wind speed (US NWS "sustained wind" window). Time-aware EWMA so the
  // window stays ~120 s regardless of call rate. This sustained value — NOT the instantaneous gust — is
  // the basis for the Beaufort readout and the gale detector: a 34 kt gust isn't a gale, and a brief lull
  // inside a 40 kt blow doesn't end one. When true wind is stale we can't confirm a gale, so the run ends.
  if (!IS_STALE(IDX_TRUE_WIND_SPEED)) {
    static uint32_t lastSustainedMs = 0;
    uint32_t nowMs = millis();
    if (isnan(sustainedTWS) || lastSustainedMs == 0) {
      sustainedTWS = TrueWindSpeedNMEA;                 // seed on first valid sample (or after a data gap)
    } else {
      float dt = (nowMs - lastSustainedMs) / 1000.0f;   // seconds since last update
      float alpha = dt / (120.0f + dt);                 // 120 s time constant
      sustainedTWS += alpha * (TrueWindSpeedNMEA - sustainedTWS);
    }
    lastSustainedMs = nowMs;

    if (sustainedTWS >= 34.0f) {                         // Beaufort 8 — gale, on the sustained average
      if (!galeActive) { galeActive = true; galeStartMs = nowMs; }
      currentGaleMinutes = (nowMs - galeStartMs) / 60000.0f;
      float galeHrs = currentGaleMinutes / 60.0f;
      if (galeHrs > longest_gale_duration_hours_alltime) longest_gale_duration_hours_alltime = galeHrs;
    } else {
      galeActive = false;
      currentGaleMinutes = 0;
    }
  } else {
    galeActive = false;     // true wind unavailable — can't prove a gale; end the run, hold last sustainedTWS for display
    currentGaleMinutes = 0;
  }
}
void saveFuelTableToNVS() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("fuel", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("Failed to open NVS for fuel table");
    Serial.println("DEBUG: Failed to open NVS!");
    return;
  }
  nvs_set_blob(nvs_handle, "fuelRPM", fuelTableRPM, sizeof(fuelTableRPM));
  Serial.print("DEBUG: Writing fuelTableGPH blob (");
  nvs_set_blob(nvs_handle, "fuelGPH", fuelTableGPH, sizeof(fuelTableGPH));
  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
}

void loadFuelTableFromNVS() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("fuel", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("Fuel: No saved table, using defaults");
    return;
  }

  size_t required_size = sizeof(fuelTableRPM);
  err = nvs_get_blob(nvs_handle, "fuelRPM", fuelTableRPM, &required_size);
  if (err != ESP_OK) {
    Serial.println("DEBUG: Failed to load fuelRPM from NVS");
    nvs_close(nvs_handle);
    return;
  }

  required_size = sizeof(fuelTableGPH);
  err = nvs_get_blob(nvs_handle, "fuelGPH", fuelTableGPH, &required_size);
  if (err != ESP_OK) {
    Serial.println("DEBUG: Failed to load fuelGPH from NVS");
  }
  nvs_close(nvs_handle);
}

bool ensureLittleFS() {
  if (littleFSMounted) {
    return true;
  }

  Serial.println("Initializing LittleFS...");

  // Start clean
  LittleFS.end();

  // Use formatOnFail=TRUE (first parameter)
  if (!LittleFS.begin(true, "/littlefs", 20, "userdata")) {
    Serial.println("CRITICAL: LittleFS begin failed even with formatOnFail=true");
    littleFSMounted = false;
    return false;
  }

  // Create mutex if it doesn't exist yet
  if (!fsMutex) {
    fsMutex = xSemaphoreCreateMutex();  // MUST be a mutex (priority inheritance + ownership)
    if (!fsMutex) {
      Serial.println("CRITICAL: fsMutex creation failed");
      littleFSMounted = false;
      return false;
    }
    // Do NOT xSemaphoreGive() a mutex here.
  }


  Serial.println("LittleFS mounted successfully");
  Serial.printf("Total: %u Used: %u\n",
                (unsigned)fsTotalBytes(),
                (unsigned)fsUsedBytes());
  littleFSMounted = true;
  return true;
}



void ensurePreferredBootPartition() {
  //called during startup, in setup() function.
  //Purpose: Decides which firmware partition to boot from and handles emergency recovery.
  //User Flow Permutations:
  //Normal Operation (GPIO41 not pressed)
  //If running from factory → switch to ota_0 (if valid firmware exists there)
  //If running from ota_0 → stay on ota_0
  //Goal: Always prefer the updated firmware in ota_0
  //Emergency Recovery (GPIO41 pressed during boot)
  //Force boot to factory partition regardless of current location
  //Clear any stuck OTA update flags in memory
  //Stay in factory mode (safe, known-good firmware)
  //Corrupted ota_0 Firmware
  //If ota_0 has invalid/corrupted firmware → stay on factory
  //Prevents boot to broken firmware
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  const esp_partition_t *current_boot = esp_ota_get_boot_partition();

  // Check GPIO41 for manual factory reset
  pinMode(41, INPUT_PULLUP);
  bool forceFactory = (digitalRead(41) == LOW);  //When pin 41 is low, forceFactory = 1

  if (forceFactory) {
    // Clear any pending update flags to prevent boot loops
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
      nvs_erase_key(nvs_handle, "update_flag");
      nvs_erase_key(nvs_handle, "target_ver");
      nvs_commit(nvs_handle);
      nvs_close(nvs_handle);
    }


    if (current_boot != factory_partition) {
      esp_ota_set_boot_partition(factory_partition);
      Serial.println("Boot: GPIO41 forced factory partition - restarting...");
      events.send("Boot: GPIO41 forced factory partition - restarting...", "console", millis());
      Serial.printf("GPIO41 state: %d\n", digitalRead(41));
      delay(1500);
      ESP.restart();
    } else {
      Serial.println("Boot: GPIO41 detected - staying in factory mode");
      events.send("Boot: GPIO41 detected - staying in factory mode", "console", millis());
      Serial.printf("GPIO41 status: %d\n", digitalRead(41));
    }


    return;  // <<<< CRITICAL: Exit function, don't run normal logic
  }

  // Check if ota_0 has valid firmware
  esp_app_desc_t app_desc;
  esp_err_t err = esp_ota_get_partition_description(ota0_partition, &app_desc);
  bool ota0_valid = (err == ESP_OK);

  if (ota0_valid && current_boot != ota0_partition) {
    esp_ota_set_boot_partition(ota0_partition);
    Serial.println("Boot: Switched to ota_0 partition !!");
    events.send("Boot: Switched to ota_0 partition", "console", millis());
  } else if (!ota0_valid && current_boot != factory_partition) {
    esp_ota_set_boot_partition(factory_partition);
    Serial.println("Boot: ota_0 invalid, using factory partition");
    events.send("Boot: ota_0 invalid, using factory partition", "console", millis());
  }
}
void sha256(const char *input, char *outputBuffer) {  // for security
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)input, strlen(input));
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);

  for (int i = 0; i < 32; ++i) {
    sprintf(outputBuffer + (i * 2), "%02x", shaResult[i]);
  }
}

void loadPasswordHash() {
  // First try to load plaintext password (for auth). Values imported from the
  // old files may carry a trailing newline (println) — trim handles both.
  if (settingExists(NK_password)) {
    String plain = settingRead(NK_password);
    plain.trim();
    strncpy(requiredPassword, plain.c_str(), sizeof(requiredPassword) - 1);
    requiredPassword[sizeof(requiredPassword) - 1] = '\0';
    Serial.println("Plaintext password loaded from NVS");
  }
  // Now load the hash (for future use)
  if (settingExists(NK_passwordHash)) {
    String hash = settingRead(NK_passwordHash);
    hash.trim();
    if (hash.length() > 0) {
      strncpy(storedPasswordHash, hash.c_str(), sizeof(storedPasswordHash) - 1);
      storedPasswordHash[sizeof(storedPasswordHash) - 1] = '\0';
      Serial.println("Password hash loaded from NVS");
      return;
    }
  }

  // If we get here, no password keys exist - set defaults
  strncpy(requiredPassword, "admin", sizeof(requiredPassword) - 1);
  sha256("admin", storedPasswordHash);
  Serial.println("No password stored, using default admin password");
}

void savePasswordHash() {
  if (settingWrite(NK_passwordHash, storedPasswordHash)) {
    Serial.println("Password hash saved to NVS");
    queueConsoleMessage("Password hash saved");
  } else {
    Serial.println("Failed to save password hash to NVS");
    queueConsoleMessage("Password hash save failed");
  }
}

void savePasswordPlaintext(const char *password) {
  if (settingWrite(NK_password, password)) {
    Serial.println("Password saved to NVS");
    queueConsoleMessage("Password saved");
  } else {
    Serial.println("Failed to save password to NVS");
    queueConsoleMessage("Password save failed");
  }
}

bool validatePassword(const char *password) {
  if (!password) return false;

  char hash[65] = { 0 };
  sha256(password, hash);

  return (strcmp(hash, storedPasswordHash) == 0);
}
// NEW - printf-style (for new code and critical fixes as of 1/20/26 that were churning heap)
void queueConsoleMessageF(const char *format, ...) {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  if (!consoleQueue || !format) return;
  char formattedMsg[128];
  va_list args;
  va_start(args, format);
  vsnprintf(formattedMsg, sizeof(formattedMsg), format, args);
  va_end(args);


  portENTER_CRITICAL(&consoleMux);
  if (consoleCount >= CONSOLE_QUEUE_SIZE) {
    consoleTail = (consoleTail + 1) % CONSOLE_QUEUE_SIZE;
    consoleCount = CONSOLE_QUEUE_SIZE - 1;
  }
  strncpy(consoleQueue[consoleHead].message, formattedMsg, 127);
  consoleQueue[consoleHead].message[127] = '\0';
  consoleQueue[consoleHead].timestamp = millis();
  consoleHead = (consoleHead + 1) % CONSOLE_QUEUE_SIZE;
  consoleCount++;
  portEXIT_CRITICAL(&consoleMux);
}
//Legacy  // Works, but no formatting benefit
void queueConsoleMessage(const char *msg) {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  if (!consoleQueue || !msg) return;

  portENTER_CRITICAL(&consoleMux);
  if (consoleCount >= CONSOLE_QUEUE_SIZE) {
    consoleTail = (consoleTail + 1) % CONSOLE_QUEUE_SIZE;
    consoleCount = CONSOLE_QUEUE_SIZE - 1;
  }
  // Copy directly into queue slot (truncate to 127)
  strncpy(consoleQueue[consoleHead].message, msg, 127);
  consoleQueue[consoleHead].message[127] = '\0';
  consoleQueue[consoleHead].timestamp = millis();
  consoleHead = (consoleHead + 1) % CONSOLE_QUEUE_SIZE;
  consoleCount++;
  portEXIT_CRITICAL(&consoleMux);
}
//legacy // OLD - String overload (keeps existing code working)
void queueConsoleMessage(const String &message) {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  queueConsoleMessage(message.c_str());
}
// Pop up to maxPop messages into provided buffers; returns count popped.
int popConsoleMessages(char outMsgs[][128], unsigned long *outTs, int maxPop) {
  if (!consoleQueue || maxPop <= 0) return 0;

  int popped = 0;

  portENTER_CRITICAL(&consoleMux);

  while (popped < maxPop && consoleCount > 0) {
    int idx = consoleTail;
    strncpy(outMsgs[popped], consoleQueue[idx].message, 127);
    outMsgs[popped][127] = '\0';
    if (outTs) outTs[popped] = consoleQueue[idx].timestamp;

    consoleTail = (consoleTail + 1) % CONSOLE_QUEUE_SIZE;
    consoleCount--;
    popped++;
  }

  portEXIT_CRITICAL(&consoleMux);

  return popped;
}
void trySendConsoleSSE(bool &sentSomething, unsigned long now) {
  if (sentSomething) return;
  if (now - lastConsoleMessageTime < CONSOLE_MESSAGE_INTERVAL) return;

  char msgs[5][128];
  unsigned long ts[5];

  int n = popConsoleMessages(msgs, ts, 5);
  if (n <= 0) return;

  // Send outside lock
  for (int i = 0; i < n; i++) {
    events.send(msgs[i], "console");
  }

  lastConsoleMessageTime = now;
  lastEventSourceSend = now;  // DELETE THIS LINE TO UNTHROTTLE CONSOLE
  sentSomething = true;
}
//NVS STUFF
void initializeNVS() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.println("NVS init issue: erasing NVS and retrying...");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      Serial.printf("ERROR: Failed to erase NVS (code %d)\n", err);
      return;
    }
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to initialize NVS (code %d)\n", err);
    return;
  }
  Serial.println("NVS initialized successfully");
  queueConsoleMessage("NVS initialized successfully");
}
// Synchronous full write — for setup() and emergency use only.
// Not safe to call from loop() — blocks for up to 200ms.
void saveNVSDataFull() {
  if (hardwarePresent != 1) return;   // sim mode (HardwarePresent=0): don't persist fake accumulators/extrema
  uint32_t _nvsT0 = millis();
  nvs_handle_t h;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &h);
  if (err != ESP_OK) {
    queueConsoleMessage("ERROR: Failed to open NVS");
    Serial.println("ERROR: Failed to open NVS");
    return;
  }
  bool chg = false;

  // Session Energy
  if (prev_ChargedEnergy != (uint32_t)ChargedEnergy)                        { nvs_set_u32(h, "ChargedEnergy",  (uint32_t)ChargedEnergy);                        prev_ChargedEnergy = (uint32_t)ChargedEnergy;                        chg = true; }
  if (prev_DischrgdEnergy != (uint32_t)DischargedEnergy)                    { nvs_set_u32(h, "DischrgdEnergy", (uint32_t)DischargedEnergy);                    prev_DischrgdEnergy = (uint32_t)DischargedEnergy;                    chg = true; }
  if (prev_AltChrgdEnergy != (uint32_t)AlternatorChargedEnergy)             { nvs_set_u32(h, "AltChrgdEnergy", (uint32_t)AlternatorChargedEnergy);             prev_AltChrgdEnergy = (uint32_t)AlternatorChargedEnergy;             chg = true; }
  if (prev_SolarEnergy != (uint32_t)SolarChargedEnergy)                     { nvs_set_u32(h, "SolarEnergy",    (uint32_t)SolarChargedEnergy);                  prev_SolarEnergy = (uint32_t)SolarChargedEnergy;                     chg = true; }
  if (prev_AltFuelUsed != (int32_t)(AlternatorFuelUsed * 10))               { nvs_set_i32(h, "AltFuelUsed",    (int32_t)(AlternatorFuelUsed * 10));             prev_AltFuelUsed = (int32_t)(AlternatorFuelUsed * 10);               chg = true; }
  if (prev_EngineFuel != (int32_t)(EngineFuelUsed * 10))                    { nvs_set_i32(h, "EngineFuel",     (int32_t)(EngineFuelUsed * 10));                 prev_EngineFuel = (int32_t)(EngineFuelUsed * 10);                    chg = true; }
  // Lifetime Energy
  if (prev_ChargedEnergy_AllTime != (uint32_t)ChargedEnergy_AllTime)        { nvs_set_u32(h, "ChrgdEng_AT",    (uint32_t)ChargedEnergy_AllTime);               prev_ChargedEnergy_AllTime = (uint32_t)ChargedEnergy_AllTime;        chg = true; }
  if (prev_DischrgdEnergy_AllTime != (uint32_t)DischargedEnergy_AllTime)    { nvs_set_u32(h, "DschrgEng_AT",   (uint32_t)DischargedEnergy_AllTime);            prev_DischrgdEnergy_AllTime = (uint32_t)DischargedEnergy_AllTime;    chg = true; }
  if (prev_AltChrgdEnergy_AllTime != (uint32_t)AlternatorChargedEnergy_AllTime) { nvs_set_u32(h, "AltChrgEng_AT", (uint32_t)AlternatorChargedEnergy_AllTime); prev_AltChrgdEnergy_AllTime = (uint32_t)AlternatorChargedEnergy_AllTime; chg = true; }
  if (prev_SolarEnergy_AllTime != (uint32_t)SolarChargedEnergy_AllTime)     { nvs_set_u32(h, "SolarEng_AT",    (uint32_t)SolarChargedEnergy_AllTime);          prev_SolarEnergy_AllTime = (uint32_t)SolarChargedEnergy_AllTime;     chg = true; }
  if (prev_AltFuelUsed_AllTime != (int32_t)(AlternatorFuelUsed_AllTime * 10)) { nvs_set_i32(h, "AltFuel_AT",  (int32_t)(AlternatorFuelUsed_AllTime * 10));    prev_AltFuelUsed_AllTime = (int32_t)(AlternatorFuelUsed_AllTime * 10); chg = true; }
  if (prev_EngineFuel_AllTime != (int32_t)(EngineFuelUsed_AllTime * 10))    { nvs_set_i32(h, "EngFuel_AT",     (int32_t)(EngineFuelUsed_AllTime * 10));        prev_EngineFuel_AllTime = (int32_t)(EngineFuelUsed_AllTime * 10);    chg = true; }
  // Runtime + Cycles
  if (prev_EngineRunTime != (int32_t)EngineRunTime)                         { nvs_set_i32(h, "EngineRunTime",  (int32_t)EngineRunTime);                        prev_EngineRunTime = (int32_t)EngineRunTime;                         chg = true; }
  if (prev_EngineCycles != (int32_t)EngineCycles)                           { nvs_set_i32(h, "EngineCycles",   (int32_t)EngineCycles);                         prev_EngineCycles = (int32_t)EngineCycles;                           chg = true; }
  if (prev_AltOnTime != (int32_t)AlternatorOnTime)                          { nvs_set_i32(h, "AltOnTime",      (int32_t)AlternatorOnTime);                     prev_AltOnTime = (int32_t)AlternatorOnTime;                          chg = true; }
  if (prev_EngineRunTime_AllTime != (int32_t)EngineRunTime_AllTime)         { nvs_set_i32(h, "EngRunTime_AT",  (int32_t)EngineRunTime_AllTime);                prev_EngineRunTime_AllTime = (int32_t)EngineRunTime_AllTime;         chg = true; }
  if (prev_perfSailSeconds != (int32_t)perfSailSeconds)                     { nvs_set_i32(h, "PerfSailSec",    (int32_t)perfSailSeconds);                      prev_perfSailSeconds = (int32_t)perfSailSeconds;                     chg = true; }
  if (prev_perfMotorSeconds != (int32_t)perfMotorSeconds)                   { nvs_set_i32(h, "PerfMotorSec",   (int32_t)perfMotorSeconds);                     prev_perfMotorSeconds = (int32_t)perfMotorSeconds;                   chg = true; }
  if (prev_EngineCycles_AllTime != (int32_t)EngineCycles_AllTime)           { nvs_set_i32(h, "EngCycles_AT",   (int32_t)EngineCycles_AllTime);                 prev_EngineCycles_AllTime = (int32_t)EngineCycles_AllTime;           chg = true; }
  if (prev_AltOnTime_AllTime != (int32_t)AlternatorOnTime_AllTime)          { nvs_set_i32(h, "AltOnTime_AT",   (int32_t)AlternatorOnTime_AllTime);             prev_AltOnTime_AllTime = (int32_t)AlternatorOnTime_AllTime;          chg = true; }
  if (prev_ChargeCycles != (int32_t)ChargeCycles)                           { nvs_set_i32(h, "ChrgCycles",     (int32_t)ChargeCycles);                         prev_ChargeCycles = (int32_t)ChargeCycles;                           chg = true; }
  if (prev_ChargeCycles_AllTime != (int32_t)ChargeCycles_AllTime)           { nvs_set_i32(h, "ChrgCyc_AT",     (int32_t)ChargeCycles_AllTime);                 prev_ChargeCycles_AllTime = (int32_t)ChargeCycles_AllTime;           chg = true; }
  // Travel + SOC session
  if (prev_TotalDist != (int32_t)TotalDistance)                             { nvs_set_i32(h, "TotalDist",      (int32_t)TotalDistance);                        prev_TotalDist = (int32_t)TotalDistance;                             chg = true; }
  if (prev_AvgSpeed != (int32_t)(AvgSpeed * 100))                           { nvs_set_i32(h, "AvgSpeed",       (int32_t)(AvgSpeed * 100));                     prev_AvgSpeed = (int32_t)(AvgSpeed * 100);                           chg = true; }
  if (prev_TotalDist_AllTime != (int32_t)TotalDistance_AllTime)             { nvs_set_i32(h, "TotDist_AT",     (int32_t)TotalDistance_AllTime);                prev_TotalDist_AllTime = (int32_t)TotalDistance_AllTime;             chg = true; }
  if (prev_AvgSpeed_AllTime != (int32_t)(AvgSpeed_AllTime * 100))           { nvs_set_i32(h, "AvgSpd_AT",      (int32_t)(AvgSpeed_AllTime * 100));             prev_AvgSpeed_AllTime = (int32_t)(AvgSpeed_AllTime * 100);           chg = true; }
  if (prev_spdAccum_AllTime  != speedAccumulator_AllTime)                   { nvs_set_blob(h, "SpdAccum_AT",   &speedAccumulator_AllTime,   sizeof(double));   prev_spdAccum_AllTime  = speedAccumulator_AllTime;                   chg = true; }
  if (prev_spdTime_AllTime   != (uint32_t)totalSpeedSampleTime_AllTime)     { nvs_set_u32(h, "SpdTime_AT",     (uint32_t)totalSpeedSampleTime_AllTime);        prev_spdTime_AllTime   = (uint32_t)totalSpeedSampleTime_AllTime;     chg = true; }
  if (prev_AvgSOC != (int32_t)(AvgSOC * 100))                               { nvs_set_i32(h, "AvgSOC",         (int32_t)(AvgSOC * 100));                       prev_AvgSOC = (int32_t)(AvgSOC * 100);                               chg = true; }
  // SOC alltime + voltage AllTime accumulators + battery state
  { uint64_t sc = (uint64_t)(socAccumulator_AllTime * 100.0f);
    if (prev_socAccum_AllTime != sc)                                         { nvs_set_u64(h, "SocAccum_AT", sc);                                               prev_socAccum_AllTime = sc;                                          chg = true; } }
  if (prev_socTime_AllTime != (uint32_t)totalSocSampleTime_AllTime)         { nvs_set_u32(h, "SocTime_AT",     (uint32_t)totalSocSampleTime_AllTime);          prev_socTime_AllTime = (uint32_t)totalSocSampleTime_AllTime;         chg = true; }
  if (prev_vltAccum_AllTime  != voltageAccumulator_AllTime)                  { nvs_set_blob(h, "VltAccum_AT",   &voltageAccumulator_AllTime, sizeof(double));   prev_vltAccum_AllTime  = voltageAccumulator_AllTime;                  chg = true; }
  if (prev_vltTime_AllTime   != (uint32_t)totalVoltageSampleTime_AllTime)    { nvs_set_u32(h, "VltTime_AT",     (uint32_t)totalVoltageSampleTime_AllTime);      prev_vltTime_AllTime   = (uint32_t)totalVoltageSampleTime_AllTime;    chg = true; }
  if (prev_SOC_percent != (int32_t)SOC_percent)                             { nvs_set_i32(h, "SOC_percent",    (int32_t)SOC_percent);                          prev_SOC_percent = (int32_t)SOC_percent;                             chg = true; }
  if (prev_CoulombCount != (int32_t)CoulombCount_Ah_scaled)                 { nvs_set_i32(h, "CoulombCount",   (int32_t)CoulombCount_Ah_scaled);               prev_CoulombCount = (int32_t)CoulombCount_Ah_scaled;                 chg = true; }
  // Health + Thermal + Learning
  if (prev_SessionDur != (uint32_t)CurrentSessionDuration)                  { nvs_set_u32(h, "SessionDur",     (uint32_t)CurrentSessionDuration);              prev_SessionDur = (uint32_t)CurrentSessionDuration;                  chg = true; }
  if (prev_MaxLoop != (int32_t)MaxLoopTime)                                  { nvs_set_i32(h, "MaxLoop",        (int32_t)MaxLoopTime);                          prev_MaxLoop = (int32_t)MaxLoopTime;                                 chg = true; }
  if (prev_MinHeap != (int32_t)MinFreeHeap)                                  { nvs_set_i32(h, "MinHeap",        (int32_t)MinFreeHeap);                          prev_MinHeap = (int32_t)MinFreeHeap;                                 chg = true; }
  if (prev_PowerCycles != (int32_t)totalPowerCycles)                        { nvs_set_i32(h, "PowerCycles",    (int32_t)totalPowerCycles);                     prev_PowerCycles = (int32_t)totalPowerCycles;                        chg = true; }
  if (prev_InsulDamage != CumulativeInsulationDamage)                       { nvs_set_blob(h, "InsulDamage",   &CumulativeInsulationDamage, sizeof(float));     prev_InsulDamage = CumulativeInsulationDamage;                       chg = true; }
  if (prev_GreaseDamage != CumulativeGreaseDamage)                          { nvs_set_blob(h, "GreaseDamage",  &CumulativeGreaseDamage,     sizeof(float));     prev_GreaseDamage = CumulativeGreaseDamage;                          chg = true; }
  if (prev_BrushDamage != CumulativeBrushDamage)                            { nvs_set_blob(h, "BrushDamage",   &CumulativeBrushDamage,      sizeof(float));     prev_BrushDamage = CumulativeBrushDamage;                            chg = true; }
  if (prev_ShuntGain != DynamicShuntGainFactor)                             { nvs_set_blob(h, "ShuntGain",     &DynamicShuntGainFactor,     sizeof(float));     prev_ShuntGain = DynamicShuntGainFactor;                             chg = true; }
  if (prev_AltZero != DynamicAltCurrentZero)                                { nvs_set_blob(h, "AltZero",       &DynamicAltCurrentZero,      sizeof(float));     prev_AltZero = DynamicAltCurrentZero;                                chg = true; }
  if (prev_LastGainTime != (uint32_t)lastGainCorrectionTime)                { nvs_set_u32(h, "LastGainTime",   (uint32_t)lastGainCorrectionTime);              prev_LastGainTime = (uint32_t)lastGainCorrectionTime;                chg = true; }
  if (prev_LastZeroTime != (uint32_t)lastAutoZeroTime)                      { nvs_set_u32(h, "LastZeroTime",   (uint32_t)lastAutoZeroTime);                    prev_LastZeroTime = (uint32_t)lastAutoZeroTime;                      chg = true; }
  if (prev_LastZeroTemp != lastAutoZeroTemp)                                 { nvs_set_blob(h, "LastZeroTemp",  &lastAutoZeroTemp,           sizeof(float));     prev_LastZeroTemp = lastAutoZeroTemp;                                chg = true; }
  if (prev_sailing_days_alltime != sailing_days_alltime)                    { nvs_set_blob(h, "SailDays_AT",   &sailing_days_alltime,       sizeof(float));     prev_sailing_days_alltime = sailing_days_alltime;                    chg = true; }
  if (prev_sailing_dist_alltime != sailing_dist_alltime)                    { nvs_set_blob(h, "SailDist_AT",   &sailing_dist_alltime,       sizeof(float));     prev_sailing_dist_alltime = sailing_dist_alltime;                    chg = true; }
  if (prev_alt_power_max_alltime_w != alt_power_max_alltime_w)              { nvs_set_blob(h, "AltPwrMax_AT",  &alt_power_max_alltime_w,    sizeof(float));     prev_alt_power_max_alltime_w = alt_power_max_alltime_w;              chg = true; }
  if (prev_solar_power_max_alltime_w != solar_power_max_alltime_w)          { nvs_set_blob(h, "SolPwrMax_AT",  &solar_power_max_alltime_w,  sizeof(float));     prev_solar_power_max_alltime_w = solar_power_max_alltime_w;          chg = true; }
  // IMU
  if (prev_imu_capsize_count != imu_capsize_count)                          { nvs_set_u32(h,  "IMU_Capsize",   imu_capsize_count);                              prev_imu_capsize_count = imu_capsize_count;                          chg = true; }
  if (prev_imu_pitchpole_count != imu_pitchpole_count)                      { nvs_set_u32(h,  "IMU_Pitchpol",  imu_pitchpole_count);                           prev_imu_pitchpole_count = imu_pitchpole_count;                      chg = true; }
  if (prev_imu_slam_count_lifetime != imu_slam_count_lifetime)              { nvs_set_u32(h,  "IMU_SlamLife",  imu_slam_count_lifetime);                        prev_imu_slam_count_lifetime = imu_slam_count_lifetime;              chg = true; }
  // Fast alt-current detector lifetime FAULT count (fleet scalar)
  if (prev_faAnomalyCount != faAnomalyCount)                                { nvs_set_u32(h,  "faAnomalyCnt",  faAnomalyCount);                                 prev_faAnomalyCount = faAnomalyCount;                                chg = true; }
  // Sea state minute counters (NVS save).
  if (prev_imu_min_moving_gentle != imu_min_moving_gentle)                  { nvs_set_u32(h,  "IMU_MinMvGnt",  imu_min_moving_gentle);                          prev_imu_min_moving_gentle = imu_min_moving_gentle;                  chg = true; }
  if (prev_imu_min_moving_moderate != imu_min_moving_moderate)              { nvs_set_u32(h,  "IMU_MinMvMod",  imu_min_moving_moderate);                        prev_imu_min_moving_moderate = imu_min_moving_moderate;              chg = true; }
  if (prev_imu_min_moving_rough != imu_min_moving_rough)                    { nvs_set_u32(h,  "IMU_MinMvRgh",  imu_min_moving_rough);                           prev_imu_min_moving_rough = imu_min_moving_rough;                    chg = true; }
  if (prev_imu_min_moving_extreme != imu_min_moving_extreme)                { nvs_set_u32(h,  "IMU_MinMvExt",  imu_min_moving_extreme);                         prev_imu_min_moving_extreme = imu_min_moving_extreme;                chg = true; }
  if (prev_imu_min_stat_gentle != imu_min_stat_gentle)                      { nvs_set_u32(h,  "IMU_MinStGnt",  imu_min_stat_gentle);                            prev_imu_min_stat_gentle = imu_min_stat_gentle;                      chg = true; }
  if (prev_imu_min_stat_moderate != imu_min_stat_moderate)                  { nvs_set_u32(h,  "IMU_MinStMod",  imu_min_stat_moderate);                          prev_imu_min_stat_moderate = imu_min_stat_moderate;                  chg = true; }
  if (prev_imu_min_stat_rough != imu_min_stat_rough)                        { nvs_set_u32(h,  "IMU_MinStRgh",  imu_min_stat_rough);                             prev_imu_min_stat_rough = imu_min_stat_rough;                        chg = true; }
  if (prev_imu_min_stat_extreme != imu_min_stat_extreme)                    { nvs_set_u32(h,  "IMU_MinStExt",  imu_min_stat_extreme);                           prev_imu_min_stat_extreme = imu_min_stat_extreme;                    chg = true; }
  if (prev_imu_heel_max_lifetime != imu_heel_max_lifetime)                  { nvs_set_blob(h, "IMU_HeelMax",   &imu_heel_max_lifetime,      sizeof(float));     prev_imu_heel_max_lifetime = imu_heel_max_lifetime;                  chg = true; }
  if (prev_imu_pitch_max_lifetime != imu_pitch_max_lifetime)                { nvs_set_blob(h, "IMU_PitchMax",  &imu_pitch_max_lifetime,     sizeof(float));     prev_imu_pitch_max_lifetime = imu_pitch_max_lifetime;                chg = true; }
  if (prev_imu_slam_peak_lifetime != imu_slam_peak_lifetime)                { nvs_set_blob(h, "IMU_SlamMax",   &imu_slam_peak_lifetime,     sizeof(float));     prev_imu_slam_peak_lifetime = imu_slam_peak_lifetime;                chg = true; }
  // imuMountOrientation / CAPSIZE_THRESHOLD_DEG / PITCHPOLE_THRESHOLD_DEG / SLAM_THRESHOLD_G
  // moved to LittleFS (Pattern B) — user-set form inputs, no longer in NVS.

  // Watermarks (session + lifetime peaks) — load block at top of loadNVSData() previously had no matching writes (F-RES-03 fix).
  if (prev_MaxSpeed != MaxSpeed)                                            { nvs_set_blob(h, "MaxSpd",        &MaxSpeed,                          sizeof(float));    prev_MaxSpeed = MaxSpeed;                                            chg = true; }
  if (prev_MaxSpeed_AllTime != MaxSpeed_AllTime)                            { nvs_set_blob(h, "MaxSpd_AT",     &MaxSpeed_AllTime,                  sizeof(float));    prev_MaxSpeed_AllTime = MaxSpeed_AllTime;                            chg = true; }
  // Longest single trip — distances stored ×100 for 0.01-nm resolution. Epoch is Unix seconds (0 = was unsynced).
  if (prev_LongestTrip_AT != (int32_t)(LongestSingleTrip_Nm_AllTime * 100)) { nvs_set_i32(h, "LongTrip_AT",    (int32_t)(LongestSingleTrip_Nm_AllTime * 100));  prev_LongestTrip_AT = (int32_t)(LongestSingleTrip_Nm_AllTime * 100); chg = true; }
  if (prev_CurrTripDist  != (int32_t)(currentTripDistanceNm * 100))         { nvs_set_i32(h, "CurrTripDist",   (int32_t)(currentTripDistanceNm * 100));         prev_CurrTripDist  = (int32_t)(currentTripDistanceNm * 100);         chg = true; }
  if (prev_CurrTripEpoch != currentTripLastUpdateEpoch)                     { nvs_set_u32(h, "CurrTripEpch",   currentTripLastUpdateEpoch);                     prev_CurrTripEpoch = currentTripLastUpdateEpoch;                     chg = true; }
  if (prev_Max24hrDist_AT != (int32_t)(Max24hrDistance_AllTime * 100))      { nvs_set_i32(h, "Max24h_AT",      (int32_t)(Max24hrDistance_AllTime * 100));       prev_Max24hrDist_AT = (int32_t)(Max24hrDistance_AllTime * 100);      chg = true; }
  if (prev_DeepAnchor_AT != (int32_t)(DeepestAnchorage_Ft_AllTime * 10))    { nvs_set_i32(h, "DeepAnchor_AT",  (int32_t)(DeepestAnchorage_Ft_AllTime * 10));    prev_DeepAnchor_AT = (int32_t)(DeepestAnchorage_Ft_AllTime * 10);    chg = true; }
  if (prev_BestUpVMG_AT != (int32_t)(best_upwind_vmg_alltime * 100))           { nvs_set_i32(h, "BestUpVMG_AT",   (int32_t)(best_upwind_vmg_alltime * 100));           prev_BestUpVMG_AT = (int32_t)(best_upwind_vmg_alltime * 100);           chg = true; }
  if (prev_GaleHrs_AT != (int32_t)(longest_gale_duration_hours_alltime * 100)) { nvs_set_i32(h, "GaleHrs_AT",     (int32_t)(longest_gale_duration_hours_alltime * 100)); prev_GaleHrs_AT = (int32_t)(longest_gale_duration_hours_alltime * 100); chg = true; }
  // Ripple analyzer worsts — persisted so they survive reboot; cleared only by the ripple panel reset
  if (prev_faSesPkpk   != (int32_t)(faSesPkpkWorstA * 100))                    { nvs_set_i32(h, "faSesPkpk",     (int32_t)(faSesPkpkWorstA * 100));                    prev_faSesPkpk   = (int32_t)(faSesPkpkWorstA * 100);                  chg = true; }
  if (prev_faSesPeakA  != (int32_t)(faSesPeakWorstA * 100))                    { nvs_set_i32(h, "faSesPeakA",    (int32_t)(faSesPeakWorstA * 100));                    prev_faSesPeakA  = (int32_t)(faSesPeakWorstA * 100);                  chg = true; }
  if (prev_faSesPeakHz != (int32_t)(faSesPeakWorstHz * 10))                    { nvs_set_i32(h, "faSesPeakHz",   (int32_t)(faSesPeakWorstHz * 10));                    prev_faSesPeakHz = (int32_t)(faSesPeakWorstHz * 10);                  chg = true; }
  // Highest Tone in Map headline — already pre-scaled (amp pk-pk ×100, freq ×10, rpm raw); cleared by Reset Worsts / Clear Map
  if (prev_faDomAmp    != (int32_t)faDomAmpAX100)                              { nvs_set_i32(h, "faDomAmp",      (int32_t)faDomAmpAX100);                              prev_faDomAmp    = (int32_t)faDomAmpAX100;                            chg = true; }
  if (prev_faDomFreq   != (int32_t)faDomFreqHzX10)                             { nvs_set_i32(h, "faDomFreq",     (int32_t)faDomFreqHzX10);                             prev_faDomFreq   = (int32_t)faDomFreqHzX10;                           chg = true; }
  if (prev_faDomRpm    != (int32_t)faDomRpm)                                   { nvs_set_i32(h, "faDomRpm",      (int32_t)faDomRpm);                                   prev_faDomRpm    = (int32_t)faDomRpm;                                 chg = true; }
  // Operating-point context for the two headline worsts (amps ×10, temp ×10 [INT32_MIN if no probe], rpm raw, epoch raw)
  if (prev_faDomAmps   != (int32_t)(faDomAmpsA * 10))                          { nvs_set_i32(h, "faDomAmps",     (int32_t)(faDomAmpsA * 10));                          prev_faDomAmps   = (int32_t)(faDomAmpsA * 10);                        chg = true; }
  if (prev_faDomTmp    != (isnan(faDomTempF) ? INT32_MIN : (int32_t)(faDomTempF * 10)))      { nvs_set_i32(h, "faDomTmp",      (isnan(faDomTempF) ? INT32_MIN : (int32_t)(faDomTempF * 10)));      prev_faDomTmp    = (isnan(faDomTempF) ? INT32_MIN : (int32_t)(faDomTempF * 10));      chg = true; }
  if (prev_faDomEp     != (int32_t)faDomEpoch)                                 { nvs_set_i32(h, "faDomEp",       (int32_t)faDomEpoch);                                 prev_faDomEp     = (int32_t)faDomEpoch;                               chg = true; }
  if (prev_faSPkAmp    != (int32_t)(faSesPkpkAmpsA * 10))                       { nvs_set_i32(h, "faSPkAmp",      (int32_t)(faSesPkpkAmpsA * 10));                       prev_faSPkAmp    = (int32_t)(faSesPkpkAmpsA * 10);                     chg = true; }
  if (prev_faSPkTmp    != (isnan(faSesPkpkTempF) ? INT32_MIN : (int32_t)(faSesPkpkTempF * 10))) { nvs_set_i32(h, "faSPkTmp",      (isnan(faSesPkpkTempF) ? INT32_MIN : (int32_t)(faSesPkpkTempF * 10))); prev_faSPkTmp    = (isnan(faSesPkpkTempF) ? INT32_MIN : (int32_t)(faSesPkpkTempF * 10)); chg = true; }
  if (prev_faSPkRpm    != (int32_t)faSesPkpkRpm)                               { nvs_set_i32(h, "faSPkRpm",      (int32_t)faSesPkpkRpm);                               prev_faSPkRpm    = (int32_t)faSesPkpkRpm;                             chg = true; }
  if (prev_faSPkEp     != (int32_t)faSesPkpkEpoch)                             { nvs_set_i32(h, "faSPkEp",       (int32_t)faSesPkpkEpoch);                             prev_faSPkEp     = (int32_t)faSesPkpkEpoch;                           chg = true; }
  if (prev_MeasAmpsMax != MeasuredAmpsMax)                                  { nvs_set_blob(h, "MAmpsMax",      &MeasuredAmpsMax,                   sizeof(float));    prev_MeasAmpsMax = MeasuredAmpsMax;                                  chg = true; }
  if (prev_MeasAmpsMax_AllTime != MeasuredAmpsMax_AllTime)                  { nvs_set_blob(h, "MAmpsMax_AT",   &MeasuredAmpsMax_AllTime,           sizeof(float));    prev_MeasAmpsMax_AllTime = MeasuredAmpsMax_AllTime;                  chg = true; }
  if (prev_RPMMax != RPMMax)                                                { nvs_set_blob(h, "RPMMax",        &RPMMax,                            sizeof(float));    prev_RPMMax = RPMMax;                                                chg = true; }
  if (prev_RPMMax_AllTime != RPMMax_AllTime)                                { nvs_set_blob(h, "RPMMax_AT",     &RPMMax_AllTime,                    sizeof(float));    prev_RPMMax_AllTime = RPMMax_AllTime;                                chg = true; }
  if (prev_IBVMax != IBVMax)                                                { nvs_set_blob(h, "IBVMax",        &IBVMax,                            sizeof(float));    prev_IBVMax = IBVMax;                                                chg = true; }
  if (prev_PeakV_AllTime != PeakVoltage_AllTime)                            { nvs_set_blob(h, "PeakV_AT",      &PeakVoltage_AllTime,               sizeof(float));    prev_PeakV_AllTime = PeakVoltage_AllTime;                            chg = true; }
  if (prev_MinVoltage != MinVoltage)                                        { nvs_set_blob(h, "MinV",          &MinVoltage,                        sizeof(float));    prev_MinVoltage = MinVoltage;                                        chg = true; }
  if (prev_MinVoltage_AllTime != MinVoltage_AllTime)                        { nvs_set_blob(h, "MinV_AT",       &MinVoltage_AllTime,                sizeof(float));    prev_MinVoltage_AllTime = MinVoltage_AllTime;                        chg = true; }
  if (prev_board_temp_max != board_temp_max_alltime)                        { nvs_set_blob(h, "BdTmpMaxAt",    &board_temp_max_alltime,            sizeof(float));    prev_board_temp_max = board_temp_max_alltime;                        chg = true; }
  if (prev_board_temp_min != board_temp_min_alltime)                        { nvs_set_blob(h, "BdTmpMinAt",    &board_temp_min_alltime,            sizeof(float));    prev_board_temp_min = board_temp_min_alltime;                        chg = true; }
  if (prev_baro_max != baro_pressure_max_alltime)                           { nvs_set_blob(h, "BaroMaxAt",     &baro_pressure_max_alltime,         sizeof(float));    prev_baro_max = baro_pressure_max_alltime;                           chg = true; }
  if (prev_baro_min != baro_pressure_min_alltime)                           { nvs_set_blob(h, "BaroMinAt",     &baro_pressure_min_alltime,         sizeof(float));    prev_baro_min = baro_pressure_min_alltime;                           chg = true; }
  if (prev_MaxTempTherm != MaxTemperatureThermistor)                        { nvs_set_blob(h, "MaxTherm",      &MaxTemperatureThermistor,          sizeof(float));    prev_MaxTempTherm = MaxTemperatureThermistor;                        chg = true; }
  if (prev_MaxTempTherm_AllTime != MaxTemperatureThermistor_AllTime)        { nvs_set_blob(h, "MaxTherm_AT",   &MaxTemperatureThermistor_AllTime,  sizeof(float));    prev_MaxTempTherm_AllTime = MaxTemperatureThermistor_AllTime;        chg = true; }
  if (prev_MaxAltTempF != MaxAlternatorTemperatureF)                        { nvs_set_blob(h, "MaxAltTempF",   &MaxAlternatorTemperatureF,         sizeof(float));    prev_MaxAltTempF = MaxAlternatorTemperatureF;                        chg = true; }
  if (prev_MaxAltTempF_AllTime != MaxAlternatorTemperatureF_AllTime)        { nvs_set_blob(h, "MAltTempF_AT",  &MaxAlternatorTemperatureF_AllTime, sizeof(float));    prev_MaxAltTempF_AllTime = MaxAlternatorTemperatureF_AllTime;        chg = true; }
  if (prev_MaxWindApp != max_wind_speed_apparent_alltime)                   { nvs_set_blob(h, "MaxWApp_AT",    &max_wind_speed_apparent_alltime,   sizeof(float));    prev_MaxWindApp = max_wind_speed_apparent_alltime;                   chg = true; }
  if (prev_MaxWindTrue != max_wind_speed_true_alltime)                      { nvs_set_blob(h, "MaxWTr_AT",     &max_wind_speed_true_alltime,       sizeof(float));    prev_MaxWindTrue = max_wind_speed_true_alltime;                      chg = true; }
  if (prev_UVToday != UVToday)                                              { nvs_set_blob(h, "UVToday",       &UVToday,                           sizeof(float));    prev_UVToday = UVToday;                                              chg = true; }
  if (prev_UVTomorrow != UVTomorrow)                                        { nvs_set_blob(h, "UVTomorrow",    &UVTomorrow,                        sizeof(float));    prev_UVTomorrow = UVTomorrow;                                        chg = true; }
  if (prev_UVDay2 != UVDay2)                                                { nvs_set_blob(h, "UVDay2",        &UVDay2,                            sizeof(float));    prev_UVDay2 = UVDay2;                                                chg = true; }

  // Barometric pressure 14-day history — 8 KB blob + head index + last-sample epoch.
  // Only written when the ring head moved since last save (a new sample landed).
  if (baroPressureHistory && prev_baroHistoryHead != baroHistoryHead) {
    nvs_set_blob(h, "BaroHist",     baroPressureHistory, BARO_HISTORY_SIZE * sizeof(uint16_t));
    nvs_set_u16(h,  "BaroHistHead", baroHistoryHead);
    nvs_set_u32(h,  "BaroHistEpch", (uint32_t)baroHistoryLastEpoch);
    prev_baroHistoryHead = baroHistoryHead;
    chg = true;
  }

  // Soft clock anchor — persist the current wall epoch so a cold power-up can seed a
  // usable timebase before any live source reports (restoreSoftClock, step 2). Written
  // whenever time is known; the field-off-edge/shutdown cadence keeps flash wear low
  // (no periodic commits — same discipline as the rest of this function).
  if (timeIsSynced && timeBase > 0) {
    nvs_set_u32(h, "SoftClockEp", (uint32_t)getCurrentTimestamp());
    chg = true;
  }

  if (chg) nvs_commit(h);
  nvs_close(h);
  lastNVSSaveTime = millis();
  uint32_t _nvsElapsed = lastNVSSaveTime - _nvsT0;
  nvsFullSaveLastMs = (_nvsElapsed > 65535UL) ? 65535 : (uint16_t)_nvsElapsed;
  if (nvsFullSaveLastMs > nvsFullSaveWorstMs) nvsFullSaveWorstMs = nvsFullSaveLastMs;
  nvsFullSaveCount++;
}

// Returns true once fieldActiveStatus has been 0 continuously for 60s + extraMs.
// Resets the moment the field turns on. Use extraMs to stagger callers so they
// don't all fire at once after a long charging session.
bool fieldOffSettled(uint32_t extraMs) {
  static unsigned long fieldOffAt = 0;
  if (fieldActiveStatus > 0) {
    fieldOffAt = 0;
    return false;
  }
  if (fieldOffAt == 0) {
    fieldOffAt = millis();
    return false;
  }
  return (millis() - fieldOffAt >= 60000UL + extraMs);
}

// Periodic phased NVS save deleted — nvs_commit() blocks Core 1 for hundreds of ms
// during sector erase and can collide with the voltage control loop on a transient.
// All NVS persistence now goes through saveNVSDataFull() at the field-off edge
// (Xregulator.ino loop) and the shutdown sequence — both run with field off so any
// commit duration is safe. See git log for the original 9-phase implementation.

void loadNVSData() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("NVS: No existing data found, using defaults");
    Serial.println("NVS: No existing data found, using defaults");
    return;
  }

  size_t required_size;
  uint32_t temp_uint32;
  int32_t temp_int32;

  // Session Energy Tracking
  if (nvs_get_u32(nvs_handle, "ChargedEnergy", &temp_uint32) == ESP_OK) ChargedEnergy = temp_uint32;
  if (nvs_get_u32(nvs_handle, "DischrgdEnergy", &temp_uint32) == ESP_OK) DischargedEnergy = temp_uint32;
  if (nvs_get_u32(nvs_handle, "AltChrgdEnergy", &temp_uint32) == ESP_OK) AlternatorChargedEnergy = temp_uint32;
  if (nvs_get_u32(nvs_handle, "SolarEnergy", &temp_uint32) == ESP_OK) SolarChargedEnergy = temp_uint32;
  if (nvs_get_i32(nvs_handle, "AltFuelUsed", &temp_int32) == ESP_OK) AlternatorFuelUsed = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "EngineFuel", &temp_int32) == ESP_OK) EngineFuelUsed = temp_int32 / 10.0f;

  // Lifetime Energy Tracking (_AllTime)
  if (nvs_get_u32(nvs_handle, "ChrgdEng_AT", &temp_uint32) == ESP_OK) ChargedEnergy_AllTime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "DschrgEng_AT", &temp_uint32) == ESP_OK) DischargedEnergy_AllTime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "AltChrgEng_AT", &temp_uint32) == ESP_OK) AlternatorChargedEnergy_AllTime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "SolarEng_AT", &temp_uint32) == ESP_OK) SolarChargedEnergy_AllTime = temp_uint32;
  if (nvs_get_i32(nvs_handle, "AltFuel_AT", &temp_int32) == ESP_OK) AlternatorFuelUsed_AllTime = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "EngFuel_AT", &temp_int32) == ESP_OK) EngineFuelUsed_AllTime = temp_int32 / 10.0f;

  // Session Runtime Tracking
  if (nvs_get_i32(nvs_handle, "EngineRunTime", &temp_int32) == ESP_OK) EngineRunTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "EngineCycles", &temp_int32) == ESP_OK) EngineCycles = temp_int32;
  if (nvs_get_i32(nvs_handle, "AltOnTime", &temp_int32) == ESP_OK) AlternatorOnTime = temp_int32;

  // Lifetime Runtime Tracking (_AllTime)
  if (nvs_get_i32(nvs_handle, "EngRunTime_AT", &temp_int32) == ESP_OK) EngineRunTime_AllTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "PerfSailSec", &temp_int32) == ESP_OK) perfSailSeconds = temp_int32;
  if (nvs_get_i32(nvs_handle, "PerfMotorSec", &temp_int32) == ESP_OK) perfMotorSeconds = temp_int32;
  if (nvs_get_i32(nvs_handle, "EngCycles_AT", &temp_int32) == ESP_OK) EngineCycles_AllTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "AltOnTime_AT", &temp_int32) == ESP_OK) AlternatorOnTime_AllTime = temp_int32;

  // Session Charge Cycles
  if (nvs_get_i32(nvs_handle, "ChrgCycles", &temp_int32) == ESP_OK) ChargeCycles = temp_int32;

  // Lifetime Charge Cycles (_AllTime)
  if (nvs_get_i32(nvs_handle, "ChrgCyc_AT", &temp_int32) == ESP_OK) ChargeCycles_AllTime = temp_int32;

  // Session Travel Statistics
  if (nvs_get_i32(nvs_handle, "TotalDist", &temp_int32) == ESP_OK) TotalDistance = temp_int32;
  if (nvs_get_i32(nvs_handle, "AvgSpeed", &temp_int32) == ESP_OK) AvgSpeed = temp_int32 / 100.0f;

  // Lifetime Travel Statistics (_AllTime)
  if (nvs_get_i32(nvs_handle, "TotDist_AT", &temp_int32) == ESP_OK) TotalDistance_AllTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "AvgSpd_AT", &temp_int32) == ESP_OK) AvgSpeed_AllTime = temp_int32 / 100.0f;

  // Longest single trip (lifetime + in-progress recovery). Boot recovery resolves once time syncs (see Step 4 hook).
  if (nvs_get_i32(nvs_handle, "LongTrip_AT", &temp_int32) == ESP_OK) LongestSingleTrip_Nm_AllTime = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "CurrTripDist", &temp_int32) == ESP_OK) currentTripDistanceNm = temp_int32 / 100.0f;
  if (nvs_get_u32(nvs_handle, "CurrTripEpch", &temp_uint32) == ESP_OK) currentTripLastUpdateEpoch = temp_uint32;
  if (currentTripDistanceNm > 0.0f || currentTripLastUpdateEpoch > 0) tripPendingRecovery = true;
  if (nvs_get_i32(nvs_handle, "Max24h_AT", &temp_int32) == ESP_OK) Max24hrDistance_AllTime = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "DeepAnchor_AT", &temp_int32) == ESP_OK) DeepestAnchorage_Ft_AllTime = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "BestUpVMG_AT", &temp_int32) == ESP_OK) best_upwind_vmg_alltime = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "GaleHrs_AT", &temp_int32) == ESP_OK) longest_gale_duration_hours_alltime = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "faSesPkpk", &temp_int32) == ESP_OK) faSesPkpkWorstA = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "faSesPeakA", &temp_int32) == ESP_OK) faSesPeakWorstA = temp_int32 / 100.0f;
  if (nvs_get_i32(nvs_handle, "faSesPeakHz", &temp_int32) == ESP_OK) faSesPeakWorstHz = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "faDomAmp", &temp_int32) == ESP_OK) faDomAmpAX100 = (uint16_t)temp_int32;     // Highest Tone in Map: pre-scaled, store/restore raw
  if (nvs_get_i32(nvs_handle, "faDomFreq", &temp_int32) == ESP_OK) faDomFreqHzX10 = (uint16_t)temp_int32;
  if (nvs_get_i32(nvs_handle, "faDomRpm", &temp_int32) == ESP_OK) faDomRpm = (uint16_t)temp_int32;
  // Operating-point context (temp sentinel INT32_MIN -> NAN, "no probe at capture")
  if (nvs_get_i32(nvs_handle, "faDomAmps", &temp_int32) == ESP_OK) faDomAmpsA = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "faDomTmp", &temp_int32) == ESP_OK) faDomTempF = (temp_int32 == INT32_MIN) ? NAN : temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "faDomEp", &temp_int32) == ESP_OK) faDomEpoch = (uint32_t)temp_int32;
  if (nvs_get_i32(nvs_handle, "faSPkAmp", &temp_int32) == ESP_OK) faSesPkpkAmpsA = temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "faSPkTmp", &temp_int32) == ESP_OK) faSesPkpkTempF = (temp_int32 == INT32_MIN) ? NAN : temp_int32 / 10.0f;
  if (nvs_get_i32(nvs_handle, "faSPkRpm", &temp_int32) == ESP_OK) faSesPkpkRpm = (uint16_t)temp_int32;
  if (nvs_get_i32(nvs_handle, "faSPkEp", &temp_int32) == ESP_OK) faSesPkpkEpoch = (uint32_t)temp_int32;

  // Speed AllTime accumulators (restore so AvgSpeed_AllTime continues correctly after reboot)
  required_size = sizeof(double);
  nvs_get_blob(nvs_handle, "SpdAccum_AT", &speedAccumulator_AllTime, &required_size);
  if (nvs_get_u32(nvs_handle, "SpdTime_AT", &temp_uint32) == ESP_OK) totalSpeedSampleTime_AllTime = temp_uint32;
  if (totalSpeedSampleTime_AllTime > 0)
    AvgSpeed_AllTime = speedAccumulator_AllTime / (float)totalSpeedSampleTime_AllTime;

  // NEW: Sailing metrics (add after other _AllTime loads)
  required_size = sizeof(float);  // ✅ Just assign, don't declare
  nvs_get_blob(nvs_handle, "SailDays_AT", &sailing_days_alltime, &required_size);
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "SailDist_AT", &sailing_dist_alltime, &required_size);
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "AltPwrMax_AT", &alt_power_max_alltime_w, &required_size);
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "SolPwrMax_AT", &solar_power_max_alltime_w, &required_size);


  // Average SOC (Session)
  if (nvs_get_i32(nvs_handle, "AvgSOC", &temp_int32) == ESP_OK) AvgSOC = temp_int32 / 100.0f;

  // Average SOC (AllTime - load ACCUMULATORS, then calculate average)
  uint64_t temp_uint64;
  if (nvs_get_u64(nvs_handle, "SocAccum_AT", &temp_uint64) == ESP_OK) {
    socAccumulator_AllTime = temp_uint64 / 100.0f;
    Serial.printf("NVS LOAD: socAccumulator_AllTime = %.2f\n", socAccumulator_AllTime);
  }

  if (nvs_get_u32(nvs_handle, "SocTime_AT", &temp_uint32) == ESP_OK) {
    totalSocSampleTime_AllTime = temp_uint32;
    Serial.printf("NVS LOAD: totalSocSampleTime_AllTime = %lu sec (%.1f hours)\n",
                  temp_uint32, temp_uint32 / 3600.0f);
  }

  // Voltage AllTime accumulators (restore so AvgVoltage_AllTime continues correctly after reboot)
  required_size = sizeof(double);
  nvs_get_blob(nvs_handle, "VltAccum_AT", &voltageAccumulator_AllTime, &required_size);
  if (nvs_get_u32(nvs_handle, "VltTime_AT", &temp_uint32) == ESP_OK) totalVoltageSampleTime_AllTime = temp_uint32;
  if (totalVoltageSampleTime_AllTime > 0)
    AvgVoltage_AllTime = voltageAccumulator_AllTime / totalVoltageSampleTime_AllTime;

  // Calculate AvgSOC_AllTime from loaded accumulators
  if (totalSocSampleTime_AllTime > 0) {
    AvgSOC_AllTime = socAccumulator_AllTime / totalSocSampleTime_AllTime;
    Serial.printf("NVS LOAD: Calculated AvgSOC_AllTime = %.2f%%\n", AvgSOC_AllTime);
  } else {
    Serial.println("NVS LOAD: No AllTime SOC history found");
  }

  // Battery State (with initialization if not found)
  if (nvs_get_i32(nvs_handle, "SOC_percent", &temp_int32) == ESP_OK) {
    SOC_percent = temp_int32;
    Serial.printf("NVS LOAD: SOC_percent = %d (%.2f%%)\n", temp_int32, temp_int32 / 100.0f);
  } else {
    // If no saved SoC, estimate from voltage
    float voltage = getBatteryVoltage();
    int estimatedSoC = 50;  // Default to 50%

    // Simple voltage-based estimation. The open-circuit thresholds below are 12V-bank values; scale
    // them by the bank class so a 24/48V bank doesn't read a flat 100%. Read the user-entered class
    // straight from NVS (NK_BatteryVoltage, the authoritative store) rather than auto-detecting from
    // the measured voltage — a sagging higher-voltage bank no longer mis-buckets. This runs in
    // loadNVSData() (before InitSystemSettings loads the vessel mirror), so on a brand-new device with
    // the key not yet seeded it falls back to 12V class — same as the prior fresh-device behavior.
    float socM = 1.0f;  // 12V class
    if (settingExists(NK_BatteryVoltage)) {
      int nomV = settingRead(NK_BatteryVoltage).toInt();
      if (nomV == 24) socM = 2.0f;
      else if (nomV == 48) socM = 4.0f;
    }
    if (voltage >= 12.7 * socM) estimatedSoC = 100;
    else if (voltage >= 12.5 * socM) estimatedSoC = 90;
    else if (voltage >= 12.4 * socM) estimatedSoC = 80;
    else if (voltage >= 12.2 * socM) estimatedSoC = 60;
    else if (voltage >= 12.0 * socM) estimatedSoC = 40;
    else if (voltage >= 11.8 * socM) estimatedSoC = 20;
    else estimatedSoC = 10;

    SOC_percent = estimatedSoC * 100;
    Serial.printf("NVS LOAD: SOC_percent NOT FOUND - estimated %d%% from voltage %.2fV\n",
                  estimatedSoC, voltage);
  }

  if (nvs_get_i32(nvs_handle, "CoulombCount", &temp_int32) == ESP_OK) {
    CoulombCount_Ah_scaled = temp_int32;
    Serial.printf("NVS LOAD: CoulombCount_Ah_scaled = %d\n", temp_int32);
  } else {
    // Initialize based on estimated SoC
    CoulombCount_Ah_scaled = (BatteryCapacity_Ah * SOC_percent) / 100;
    // CoulombCount_Ah_scaled = (BatteryCapacity_Ah * SOC_percent) / 10000; ChatGPT suggests this!!

    Serial.printf("NVS LOAD: CoulombCount NOT FOUND - initialized to %d based on SoC\n",
                  CoulombCount_Ah_scaled);
  }

  // Session Health Stats (✅ restore to prior-session variables)
  if (nvs_get_u32(nvs_handle, "SessionDur", &temp_uint32) == ESP_OK) LastSessionDuration = temp_uint32;
  if (nvs_get_i32(nvs_handle, "MaxLoop", &temp_int32) == ESP_OK) LastSessionMaxLoopTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "MinHeap", &temp_int32) == ESP_OK) lastSessionMinHeap = temp_int32;

  // System Health Counters
  if (nvs_get_i32(nvs_handle, "PowerCycles", &temp_int32) == ESP_OK) totalPowerCycles = temp_int32;

  // Thermal Stress
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "InsulDamage", &CumulativeInsulationDamage, &required_size);
  nvs_get_blob(nvs_handle, "GreaseDamage", &CumulativeGreaseDamage, &required_size);
  nvs_get_blob(nvs_handle, "BrushDamage", &CumulativeBrushDamage, &required_size);

  // Dynamic Learning
  nvs_get_blob(nvs_handle, "ShuntGain", &DynamicShuntGainFactor, &required_size);
  nvs_get_blob(nvs_handle, "AltZero", &DynamicAltCurrentZero, &required_size);
  if (nvs_get_u32(nvs_handle, "LastGainTime", &temp_uint32) == ESP_OK) lastGainCorrectionTime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "LastZeroTime", &temp_uint32) == ESP_OK) lastAutoZeroTime = temp_uint32;
  nvs_get_blob(nvs_handle, "LastZeroTemp", &lastAutoZeroTemp, &required_size);

  // IMU Lifetime Counters
  if (nvs_get_u32(nvs_handle, "IMU_Capsize", &temp_uint32) == ESP_OK) imu_capsize_count = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_Pitchpol", &temp_uint32) == ESP_OK) imu_pitchpole_count = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_SlamLife", &temp_uint32) == ESP_OK) imu_slam_count_lifetime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "faAnomalyCnt", &temp_uint32) == ESP_OK) faAnomalyCount = temp_uint32;

  // Sea state minute counters (NVS load).
  if (nvs_get_u32(nvs_handle, "IMU_MinMvGnt",  &temp_uint32) == ESP_OK) imu_min_moving_gentle   = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinMvMod",  &temp_uint32) == ESP_OK) imu_min_moving_moderate = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinMvRgh",  &temp_uint32) == ESP_OK) imu_min_moving_rough    = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinMvExt",  &temp_uint32) == ESP_OK) imu_min_moving_extreme  = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinStGnt",  &temp_uint32) == ESP_OK) imu_min_stat_gentle     = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinStMod",  &temp_uint32) == ESP_OK) imu_min_stat_moderate   = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinStRgh",  &temp_uint32) == ESP_OK) imu_min_stat_rough      = temp_uint32;
  if (nvs_get_u32(nvs_handle, "IMU_MinStExt",  &temp_uint32) == ESP_OK) imu_min_stat_extreme    = temp_uint32;

  // IMU Lifetime Maximums
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "IMU_HeelMax", &imu_heel_max_lifetime, &required_size);
  nvs_get_blob(nvs_handle, "IMU_PitchMax", &imu_pitch_max_lifetime, &required_size);
  nvs_get_blob(nvs_handle, "IMU_SlamMax", &imu_slam_peak_lifetime, &required_size);

  // imuMountOrientation now loads from /vessel_info.json in InitSystemSettings.
  // CAPSIZE_THRESHOLD_DEG / PITCHPOLE_THRESHOLD_DEG / SLAM_THRESHOLD_G now load from
  // their own NVS "settings" keys in InitSystemSettings (Pattern B).

  // Extrema, environment maxima, and UV forecast
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxSpd",      &MaxSpeed,                         &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxSpd_AT",   &MaxSpeed_AllTime,                 &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MAmpsMax",    &MeasuredAmpsMax,                  &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MAmpsMax_AT", &MeasuredAmpsMax_AllTime,          &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "RPMMax",      &RPMMax,                           &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "RPMMax_AT",   &RPMMax_AllTime,                   &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "IBVMax",      &IBVMax,                           &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "PeakV_AT",    &PeakVoltage_AllTime,              &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MinV",        &MinVoltage,                       &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MinV_AT",     &MinVoltage_AllTime,               &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "BdTmpMaxAt",  &board_temp_max_alltime,           &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "BdTmpMinAt",  &board_temp_min_alltime,           &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "BaroMaxAt",   &baro_pressure_max_alltime,        &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "BaroMinAt",   &baro_pressure_min_alltime,        &required_size);
  // Guard against NaN/Inf written by older firmware before the runtime isnan
  // guard existed. Once a watermark turns NaN, comparisons never update it again.
  if (!isfinite(board_temp_max_alltime))    board_temp_max_alltime    = -999.0f;
  if (!isfinite(board_temp_min_alltime))    board_temp_min_alltime    =  999.0f;
  if (!isfinite(baro_pressure_max_alltime)) baro_pressure_max_alltime =    0.0f;
  if (!isfinite(baro_pressure_min_alltime)) baro_pressure_min_alltime = 9999.0f;
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxTherm",    &MaxTemperatureThermistor,         &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxTherm_AT", &MaxTemperatureThermistor_AllTime, &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxAltTempF", &MaxAlternatorTemperatureF,        &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MAltTempF_AT", &MaxAlternatorTemperatureF_AllTime, &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxWApp_AT",  &max_wind_speed_apparent_alltime,  &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "MaxWTr_AT",   &max_wind_speed_true_alltime,      &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "UVToday",     &UVToday,                          &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "UVTomorrow",  &UVTomorrow,                       &required_size);
  required_size = sizeof(float); nvs_get_blob(nvs_handle, "UVDay2",      &UVDay2,                           &required_size);

  // Barometric pressure 14-day history. Buffer is ps_malloc'd in setup() before this
  // function runs, so it's safe to load directly into it. Missing-key paths leave the
  // buffer zeroed (its memset default), which JS treats as "no sample".
  if (baroPressureHistory) {
    required_size = BARO_HISTORY_SIZE * sizeof(uint16_t);
    nvs_get_blob(nvs_handle, "BaroHist", baroPressureHistory, &required_size);
    nvs_get_u16(nvs_handle, "BaroHistHead", (uint16_t *)&baroHistoryHead);
    uint32_t epochTmp = 0;
    nvs_get_u32(nvs_handle, "BaroHistEpch", &epochTmp);
    baroHistoryLastEpoch = (time_t)epochTmp;
    prev_baroHistoryHead = baroHistoryHead;
  }

  nvs_close(nvs_handle);
  //queueConsoleMessage("NVS: Persistent data loaded successfully");
}
void initNVSCache() {
  // Session cache
  prev_ChargedEnergy = (uint32_t)ChargedEnergy;
  prev_DischrgdEnergy = (uint32_t)DischargedEnergy;
  prev_AltChrgdEnergy = (uint32_t)AlternatorChargedEnergy;
  prev_SolarEnergy = (uint32_t)SolarChargedEnergy;
  prev_AltFuelUsed = (int32_t)(AlternatorFuelUsed * 10);
  prev_EngineFuel = (int32_t)(EngineFuelUsed * 10);
  prev_EngineRunTime = (int32_t)EngineRunTime;
  prev_EngineCycles = (int32_t)EngineCycles;
  prev_AltOnTime = (int32_t)AlternatorOnTime;
  prev_ChargeCycles = (int32_t)ChargeCycles;
  prev_TotalDist = (int32_t)TotalDistance;
  prev_AvgSpeed = (int32_t)(AvgSpeed * 100);
  prev_AvgSOC = (int32_t)(AvgSOC * 100);

  // Lifetime cache (_AllTime)
  prev_ChargedEnergy_AllTime = (uint32_t)ChargedEnergy_AllTime;
  prev_DischrgdEnergy_AllTime = (uint32_t)DischargedEnergy_AllTime;
  prev_AltChrgdEnergy_AllTime = (uint32_t)AlternatorChargedEnergy_AllTime;
  prev_SolarEnergy_AllTime = (uint32_t)SolarChargedEnergy_AllTime;
  prev_AltFuelUsed_AllTime = (int32_t)(AlternatorFuelUsed_AllTime * 10);
  prev_EngineFuel_AllTime = (int32_t)(EngineFuelUsed_AllTime * 10);
  prev_EngineRunTime_AllTime = (int32_t)EngineRunTime_AllTime;
  prev_perfSailSeconds = (int32_t)perfSailSeconds;
  prev_perfMotorSeconds = (int32_t)perfMotorSeconds;
  prev_EngineCycles_AllTime = (int32_t)EngineCycles_AllTime;
  prev_AltOnTime_AllTime = (int32_t)AlternatorOnTime_AllTime;
  prev_ChargeCycles_AllTime = (int32_t)ChargeCycles_AllTime;
  prev_TotalDist_AllTime = (int32_t)TotalDistance_AllTime;
  prev_AvgSpeed_AllTime = (int32_t)(AvgSpeed_AllTime * 100);
  prev_spdAccum_AllTime = speedAccumulator_AllTime;
  prev_spdTime_AllTime  = (uint32_t)totalSpeedSampleTime_AllTime;
  prev_vltAccum_AllTime = voltageAccumulator_AllTime;
  prev_vltTime_AllTime  = (uint32_t)totalVoltageSampleTime_AllTime;
  prev_AvgSOC_AllTime = (int32_t)(AvgSOC_AllTime * 100);

  // Battery state
  prev_SOC_percent = (int32_t)SOC_percent;
  prev_CoulombCount = (int32_t)CoulombCount_Ah_scaled;

  // Session health
  prev_SessionDur = (uint32_t)CurrentSessionDuration;
  prev_MaxLoop = (int32_t)MaxLoopTime;
  prev_MinHeap = (int32_t)MinFreeHeap;

  // System health
  prev_PowerCycles = (int32_t)totalPowerCycles;

  // Thermal stress
  prev_InsulDamage = CumulativeInsulationDamage;
  prev_GreaseDamage = CumulativeGreaseDamage;
  prev_BrushDamage = CumulativeBrushDamage;

  // Dynamic learning
  prev_ShuntGain = DynamicShuntGainFactor;
  prev_AltZero = DynamicAltCurrentZero;
  prev_LastGainTime = (uint32_t)lastGainCorrectionTime;
  prev_LastZeroTime = (uint32_t)lastAutoZeroTime;
  prev_LastZeroTemp = lastAutoZeroTemp;

  // Sailing metrics cached
  prev_sailing_days_alltime = sailing_days_alltime;
  prev_sailing_dist_alltime = sailing_dist_alltime;
  prev_alt_power_max_alltime_w = alt_power_max_alltime_w;
  prev_solar_power_max_alltime_w = solar_power_max_alltime_w;

  // IMU cache
  prev_imu_capsize_count = imu_capsize_count;
  prev_imu_pitchpole_count = imu_pitchpole_count;
  prev_imu_slam_count_lifetime = imu_slam_count_lifetime;
  prev_faAnomalyCount = faAnomalyCount;
  prev_imu_heel_max_lifetime = imu_heel_max_lifetime;
  prev_imu_pitch_max_lifetime = imu_pitch_max_lifetime;
  prev_imu_slam_peak_lifetime = imu_slam_peak_lifetime;
  // Sea state minute counters (prev_ shadow sync).
  prev_imu_min_moving_gentle   = imu_min_moving_gentle;
  prev_imu_min_moving_moderate = imu_min_moving_moderate;
  prev_imu_min_moving_rough    = imu_min_moving_rough;
  prev_imu_min_moving_extreme  = imu_min_moving_extreme;
  prev_imu_min_stat_gentle     = imu_min_stat_gentle;
  prev_imu_min_stat_moderate   = imu_min_stat_moderate;
  prev_imu_min_stat_rough      = imu_min_stat_rough;
  prev_imu_min_stat_extreme    = imu_min_stat_extreme;
  prev_MaxSpeed             = MaxSpeed;
  prev_MaxSpeed_AllTime     = MaxSpeed_AllTime;
  prev_LongestTrip_AT       = (int32_t)(LongestSingleTrip_Nm_AllTime * 100);
  prev_CurrTripDist         = (int32_t)(currentTripDistanceNm * 100);
  prev_CurrTripEpoch        = currentTripLastUpdateEpoch;
  prev_Max24hrDist_AT       = (int32_t)(Max24hrDistance_AllTime * 100);
  prev_DeepAnchor_AT        = (int32_t)(DeepestAnchorage_Ft_AllTime * 10);
  prev_BestUpVMG_AT         = (int32_t)(best_upwind_vmg_alltime * 100);
  prev_GaleHrs_AT           = (int32_t)(longest_gale_duration_hours_alltime * 100);
  prev_faSesPkpk            = (int32_t)(faSesPkpkWorstA * 100);
  prev_faSesPeakA           = (int32_t)(faSesPeakWorstA * 100);
  prev_faSesPeakHz          = (int32_t)(faSesPeakWorstHz * 10);
  prev_faDomAmp             = (int32_t)faDomAmpAX100;
  prev_faDomFreq            = (int32_t)faDomFreqHzX10;
  prev_faDomRpm             = (int32_t)faDomRpm;
  prev_faDomAmps            = (int32_t)(faDomAmpsA * 10);
  prev_faDomTmp             = (isnan(faDomTempF) ? INT32_MIN : (int32_t)(faDomTempF * 10));
  prev_faDomEp              = (int32_t)faDomEpoch;
  prev_faSPkAmp             = (int32_t)(faSesPkpkAmpsA * 10);
  prev_faSPkTmp             = (isnan(faSesPkpkTempF) ? INT32_MIN : (int32_t)(faSesPkpkTempF * 10));
  prev_faSPkRpm             = (int32_t)faSesPkpkRpm;
  prev_faSPkEp              = (int32_t)faSesPkpkEpoch;
  prev_MeasAmpsMax          = MeasuredAmpsMax;
  prev_MeasAmpsMax_AllTime  = MeasuredAmpsMax_AllTime;
  prev_RPMMax               = RPMMax;
  prev_RPMMax_AllTime       = RPMMax_AllTime;
  prev_IBVMax               = IBVMax;
  prev_PeakV_AllTime        = PeakVoltage_AllTime;
  prev_MinVoltage           = MinVoltage;
  prev_MinVoltage_AllTime   = MinVoltage_AllTime;
  prev_board_temp_max       = board_temp_max_alltime;
  prev_board_temp_min       = board_temp_min_alltime;
  prev_baro_max             = baro_pressure_max_alltime;
  prev_baro_min             = baro_pressure_min_alltime;
  prev_MaxTempTherm         = MaxTemperatureThermistor;
  prev_MaxTempTherm_AllTime = MaxTemperatureThermistor_AllTime;
  prev_MaxAltTempF          = MaxAlternatorTemperatureF;
  prev_MaxAltTempF_AllTime  = MaxAlternatorTemperatureF_AllTime;
  prev_MaxWindApp           = max_wind_speed_apparent_alltime;
  prev_MaxWindTrue          = max_wind_speed_true_alltime;
  prev_UVToday              = UVToday;
  prev_UVTomorrow           = UVTomorrow;
  prev_UVDay2               = UVDay2;
}
// Helper function to get human-readable subtype names
String getSubtypeString(esp_partition_type_t type, esp_partition_subtype_t subtype) {
  if (type == ESP_PARTITION_TYPE_APP) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "factory";
      case ESP_PARTITION_SUBTYPE_APP_OTA_0: return "ota_0";
      case ESP_PARTITION_SUBTYPE_APP_OTA_1: return "ota_1";
      default: return "app_unknown";
    }
  } else if (type == ESP_PARTITION_TYPE_DATA) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_DATA_NVS: return "nvs";
      case ESP_PARTITION_SUBTYPE_DATA_OTA: return "otadata";
      case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: return "littlefs";
      case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: return "spiffs";
      case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "coredump";
      default: return "data_unknown";
    }
  }
  return "unknown";
}
// Helper to verify expected partitions exist with correct sizes
void checkExpectedPartition(const char *name, esp_partition_type_t type, esp_partition_subtype_t subtype, size_t expectedSize) {
  const esp_partition_t *partition = esp_partition_find_first(type, subtype, name);
  if (partition) {
    bool sizeOK = (partition->size == expectedSize);
    Serial.printf("  ✅ %s: Found at 0x%X, size %d bytes %s\n",
                  name, partition->address, partition->size,
                  sizeOK ? "✓" : "❌ SIZE MISMATCH!");
  } else {
    Serial.printf("  ❌ %s: NOT FOUND!\n", name);
  }
}
