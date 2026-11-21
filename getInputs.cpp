#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

// ==============================================================================
// FTMS-Daten
extern volatile uint32_t rxControlMode;
extern volatile uint32_t rxTargetPower;
extern volatile float rxTargetSlope;
extern volatile bool rxBtConnected;

void getFTMSInputs()
{
  // Threadsichere Zuweisung an die Hauptdatenstruktur von Core 1
  systemData.controlMode = (int32_t)rxControlMode;
  systemData.targetPowerWatt = (int32_t)rxTargetPower;
  systemData.targetSlope = rxTargetSlope;
  systemData.btClientConnected = rxBtConnected;
}

// ==============================================================================
// Pulsgurt-Daten
extern volatile int32_t rxHeartRate;
extern volatile int32_t rxHrBattery;
extern volatile bool rxHrConnected;

void getPulsInputs()
{
  // Threadsichere Übergabe an die Datenstruktur von Core 1
  systemData.heartRateBPM = rxHeartRate;
  systemData.heartRateBatteryPercent = rxHrBattery;
  systemData.hrSensorConnected = rxHrConnected;
}

// ==============================================================================
// Volatile Variablen für die schnellen Interrupt-Zähler
volatile uint32_t isrEncoderCount = 0;
volatile uint32_t isrLastCadenceTime = 0;
volatile uint32_t isrCadenceDeltaUs = 0;
volatile bool isrPedalDetected = false;

// Zeitstempel für die Software-Entprellung der Taster
static uint32_t lastGearChangeTime = 0;

// Interrupt-Routine für den Kurbelsensor (Trittfrequenz)
void IRAM_ATTR cadenceISR()
{
  uint32_t now = micros();
  // Entprellung: Ein Trittfrequenz-Signal kann physikalisch nicht schneller als alle 150ms kommen (ca. 400 RPM)
  if (now - isrLastCadenceTime > (DEBOUNCE_DELAY * 1000))
    {
      isrCadenceDeltaUs = now - isrLastCadenceTime;
      isrLastCadenceTime = now;
      isrPedalDetected = true;
    }
}

// Interrupt-Routine für den Inkrementalgeber (Schritt-Zähler Stellmotor)
void IRAM_ATTR encoderISR()
{
  isrEncoderCount++;
}

// Interrupt-Routine für den Referenz-Endschalter (Nullpunkt Stellmotor)
void IRAM_ATTR referenceISR()
{
  isrEncoderCount = 0; // Sofortige Kalibrierung der Motor-Ist-Position
}

void getGPIOInputs()
{
  // --- 1. KADENZ BERECHNEN ---
  uint32_t now = micros();

  // Falls länger als 3 Sekunden (3.000.000 µs) nicht getreten wurde, ist die Kadenz = 0
  if (now - isrLastCadenceTime > 3000000)
    {
      systemData.cadenceRPM = 0;
    }
  else if (isrCadenceDeltaUs > 0)
    {
      // Formel: (60 Sekunden * 1.000.000 µs) / Delta-Zeit in µs
      systemData.cadenceRPM = 60000000 / isrCadenceDeltaUs;
    }

  // Pedal-Impuls an die App signalisieren
  systemData.pedalPulseDetected = isrPedalDetected;
  isrPedalDetected = false; // Zurücksetzen für den nächsten Impuls

  // --- 2. GANGSCHALTUNG EINLESEN (Prellfrei) ---
  // Drücken zieht den Pin auf LOW (Masse)
  bool plusBtnPressed = (digitalRead(GEAR_PLUS) == LOW);
  bool minusBtnPressed = (digitalRead(GEAR_MINUS) == LOW);

  // Wenn ein Taster gedrückt ist UND die Entprellzeit (150ms) abgelaufen ist
  if ((plusBtnPressed || minusBtnPressed) && (nowMs - lastGearChangeTime > DEBOUNCE_DELAY))
    {
      if (plusBtnPressed)
        {
          // Hochschalten (maximal bis GEAR_COUNT / Gang 15)
          if (systemData.currentGear < GEAR_COUNT)
            {
              systemData.currentGear++;
              Serial.printf("-> Hochschalten! Neuer Gang: %d\n", systemData.currentGear);
            }
        }
      else if (minusBtnPressed)
        {
          // Runterschalten (minimal bis Gang 1)
          if (systemData.currentGear > 1)
            {
              systemData.currentGear--;
              Serial.printf("-> Runterschalten! Neuer Gang: %d\n", systemData.currentGear);
            }
        }

      // Letzten Schaltzeitpunkt sichern
      lastGearChangeTime = nowMs;
    }

  // --- 3. ENCODER-POSITION SPEICHERN ---
  // Wir weisen dem Ergometer die aktuell physikalisch gezählte Motorposition zu
  systemData.targetMotorPosition = isrEncoderCount;
}