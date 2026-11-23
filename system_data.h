#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>

const uint32_t MATRIX_ROWS = 21;
const uint32_t MATRIX_COLS = 5;
const uint32_t COL_CHAR_LIMIT = 10;

// Die neuen, sprechenden Betriebsmodi des Smartbikes
enum class OpMode : int32_t {
  HOMING = 0, // Referenzfahrt nach dem Start
  MANUAL = 1, // Manueller Modus über Gangschaltung
  PULS = 2,   // Puls-Training (Cardio-Modus)
  SIMU = 3    // Interaktive Strecken-Simulation (Kinomap)
};

// Zustände für die sanfte 100ms-Motor-Rampe
enum class MotorState : int32_t {
  STOPPED = 0,
  ACCEL = 1,
  RUNNING = 2,
  DECEL = 3,
  BACKLASH = 4
};

// ==============================================================================
// 1. SCHNITTSTELLE: Bluetooth FTMS (Kinomap)
// ==============================================================================
struct FTMSServiceData
{
  bool btClientConnected;   // Verbindung zur App aktiv
  float targetSlope;        // Soll-Steigung von der App in %
  uint32_t targetPowerWatt; // Soll-Leistung von der App im ERG-Modus
  OpMode rxControlMode;     // Von der App angeforderter Modus (Simu/Manual)
};

// ==============================================================================
// 2. SCHNITTSTELLE: BLE Pulsgurt (Heart Rate Monitor)
// ==============================================================================
struct PulsGurtData
{
  bool hrSensorConnected;    // Verbindung zum Gurt aktiv
  uint32_t heartRateBPM;     // Aktueller Live-Puls (Rohwert)
  uint32_t heartRateAverage; // Gefilterter Puls über 10 Sekunden
  uint32_t batteryPercent;   // Batterieladung in %

  uint32_t hrBuffer[HR_BUFFER_SIZE];
  uint32_t bufferIndex;
  uint32_t hrSum;
};

// ==============================================================================
// 3. SCHNITTSTELLE: Physische GPIOs (Hardware-Inputs & -Outputs)
// ==============================================================================
struct GPIOHardwareData
{
  // Live-Zustände der physikalischen Pins (Wichtig für Diagnose und Logik)
  bool cadenceRawPulse;        // Momentaner Zustand von GPIO 10 (Reed-Sensor)
  bool encoderRawPulse;        // Momentaner Zustand von GPIO 11 (Motor-Inkrementalgeber)
  bool referenceSwitchPressed; // Momentaner Zustand von GPIO 12 (Referenzschalter, entprellt/aktiviert)

  bool pedalPulseDetected;      // Signalisiert, dass ein Kurbel-Umdrehungs-Impuls erkannt wurde (ISR-Trigger)
  uint32_t actualMotorPosition; // Aktuell gezählte Schrittzahl des Inkrementalgebers (Ist-Wert)
  uint32_t targetMotorPosition; // Anzufahrende Ziel-Schrittzahl des Stellmotors (Soll-Wert)

  bool gpioData.plusBtnPressed;
  bool gpioData.minusBtnPressed;
  uint32_t lastGearChangeTime;

  MotorState currentMotorState;
  uint32_t motorStateStartTime;
  bool activeDirectionPos;
};

// ==============================================================================
// 4. KERN: Berechnete Ergometer-Fahrdaten (Fahrphysik & Status)
// ==============================================================================
struct ErgometerData
{
  OpMode controlMode;           // Aktiver Steuerungsmodus (Homing, Manual, Puls, Simu)
  uint32_t currentGear;         // Aktueller virtueller Gang (1 bis 13)
  float speedKmH;               // Berechnete virtuelle Geschwindigkeit in km/h
  float targetTorqueNm;         // Berechnetes Soll-Bremsmoment in Nm
  uint32_t currentPowerWatt;    // Berechnete / getretene Leistung in Watt
  uint32_t distanceMillimeters; // Gesamtstrecke in Millimetern

  uint32_t lastPulsRegulatorTime = 0;
};

// ==============================================================================
// 5. DIAGNOSE & VISUALISIERUNG: TFT Onboard-Display
// ==============================================================================
struct DisplayData
{
  // Speicher für Labels, z.B. "RPM" (Max 5 Zeichen + Nullterminator)
  char displayLabels[MATRIX_ROWS][MATRIX_COLS][6];

  // Speicher für Ganzzahlen-Werte, die den Labels zugeordnet sind
  uint32_t displayValues[MATRIX_ROWS][MATRIX_COLS];

  // Der fertige, formatierte String, den das Display zeichnet
  char displayMatrix[MATRIX_ROWS][MATRIX_COLS][COL_CHAR_LIMIT + 1];
};

// ==============================================================================
// INLINE-DEFINITIONEN (Echte Definitionen direkt im Header!)
// ==============================================================================
inline FTMSServiceData ftmsData;
inline PulsGurtData pulsGurtData;
inline GPIOHardwareData gpioData;
inline ErgometerData systemData;
inline DisplayData displayData;

#endif
