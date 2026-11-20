#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>

const uint32_t MATRIX_ROWS = 21;
const uint32_t MATRIX_COLS = 5;
const uint32_t COL_CHAR_LIMIT = 10;

struct ErgometerData
{
  // Physische Trainingsdaten
  uint32_t cadenceRPM;
  uint32_t heartRateBPM;
  bool pedalPulseDetected;
  uint32_t currentGear;
  uint32_t targetMotorPosition;
  uint32_t controlMode;
  uint32_t speedKmH;
  uint32_t currentPowerWatt;

  // Speicher für Labels und Werte
  char displayLabels[MATRIX_ROWS][MATRIX_COLS]
                    [6]; // Max 5 Zeichen für Label (z.B. "RPM")
  uint32_t displayValues[MATRIX_ROWS][MATRIX_COLS];

  // Der fertige String für das Display
  char displayMatrix[MATRIX_ROWS][MATRIX_COLS][COL_CHAR_LIMIT + 1];
};

extern ErgometerData systemData;

#endif
