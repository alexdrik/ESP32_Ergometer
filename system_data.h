#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>

const uint32_t MATRIX_ROWS = 21;
const uint32_t MATRIX_COLS = 5;
const uint32_t COL_CHAR_LIMIT = 10;

struct ErgometerData
{
  // Physische Trainingsdaten
  bool pedalPulseDetected;
  bool btClientCOnnected; // Status des FTMS-Servers
  bool hrSensorConnected; // Status des Pulsgurts

  int32_t cadenceRPM;
  int32_t heartRateBPM;
  int32_t currentGear;
  int32_t targetMotorPosition;
  int32_t controlMode;
  int32_t currentPowerWatt;
  int32_t targetPowerWatt;
  int32_t distanceMillimeters;

  float targetSlope;    // Steigung in % (z.B. -1.5f oder 5.5f)
  float speedKmH;       // Geschwindigkeit (z.B. 28.4f)
  float targetTorqueNm; // Bremsmoment vom Ergometer (z.B. 45.0f)

  // Speicher für Labels und Werte
  char displayLabels[MATRIX_ROWS][MATRIX_COLS]
                    [6]; // Max 5 Zeichen für Label (z.B. "RPM")
  uint32_t displayValues[MATRIX_ROWS][MATRIX_COLS];

  // Der fertige String für das Display
  char displayMatrix[MATRIX_ROWS][MATRIX_COLS][COL_CHAR_LIMIT + 1];
};

extern ErgometerData systemData;

#endif
