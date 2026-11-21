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

// Statische Variablen für den gleitenden Puls-Mittelwert (10 Sekunden)
const int32_t BUFFER_SIZE = 10;
static int32_t hrBuffer[BUFFER_SIZE] = {0};
static int32_t bufferIndex = 0;
static uint32_t lastBufferTime = 0;
static bool bufferFull = false;

void getPulsInputs()
{
  uint32_t nowMs = millis();

  // 1. Threadsichere Übernahme der Rohdaten von Core 0
  systemData.heartRateBPM = rxHeartRate;
  systemData.heartRateBatteryPercent = rxHrBattery;
  systemData.hrSensorConnected = rxHrConnected;

  // 2. Signalverarbeitung: Gleitender Mittelwert-Filter
  if (systemData.hrSensorConnected && systemData.heartRateBPM > 0)
    {
      // Jede Sekunde (1000ms) schreiben wir den aktuellen Puls in den Ringpuffer
      if (nowMs - lastBufferTime >= 1000)
        {
          lastBufferTime = nowMs;
          hrBuffer[bufferIndex] = systemData.heartRateBPM;
          bufferIndex++;

          if (bufferIndex >= BUFFER_SIZE)
            {
              bufferIndex = 0;
              bufferFull = true; // Puffer ist voll, wir haben stabile 10 Sekunden gefiltert
            }
        }

      // Mittelwert berechnen
      if (bufferFull)
        {
          int32_t hrSum = 0;
          for (int32_t i = 0; i < BUFFER_SIZE; i++)
            {
              hrSum += hrBuffer[i];
            }
          systemData.heartRateBPM_Average = hrSum / BUFFER_SIZE;
        }
      else
        {
          // Solange der Puffer noch nicht voll ist, nehmen wir direkt den aktuellen Puls als Fallback
          systemData.heartRateBPM_Average = systemData.heartRateBPM;
        }
    }
  else
    {
      // Wenn kein Sensor verbunden ist, setzen wir den Filter zurück
      memset(hrBuffer, 0, sizeof(hrBuffer));
      bufferIndex = 0;
      bufferFull = false;
      systemData.heartRateBPM_Average = 0;
    }
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