#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

// ==============================================================================
void getFTMSInputs()
{
  // Threadsichere Zuweisung an die Hauptdatenstruktur von Core 1
  systemData.controlMode = (int32_t)rxControlMode;
  systemData.targetPowerWatt = (int32_t)rxTargetPower;
  systemData.targetSlope = rxTargetSlope;
  systemData.btClientConnected = rxBtConnected;
}

// ==============================================================================
void getPulsInputs()
{
  // Da Core 0 den Puls-Filter live und synchron bei jedem BLE-Empfang berechnet,
  // kopieren wir die Daten hier nur noch threadsicher für Core 1.
  pulsGurtData.hrSensorConnected = rxHrConnected;
  pulsGurtData.batteryPercent = rxHrBattery;

  // Wenn getrennt, fallen wir auf 90 zurück
  if (!pulsGurtData.hrSensorConnected)
    {
      pulsGurtData.heartRateBPM = 90;
      pulsGurtData.heartRateAverage = 90;
    }
}

// ==============================================================================
// Interrupt-Routine für den Kurbelsensor (Trittfrequenz)
void IRAM_ATTR cadenceISR()
{
  uint32_t now = micros();
  // Entprellung: Ein Trittfrequenz-Signal kann physikalisch nicht schneller als alle 150ms kommen (ca. 400 RPM)
  if (now - gpioData.isrLastCadenceTime > (DEBOUNCE_DELAY * 1000))
    {
      gpioData.isrCadenceDeltaUs = now - gpioData.isrLastCadenceTime;
      gpioData.isrLastCadenceTime = now;
      gpioData.isrPedalDetected = true;
    }
}

// ==============================================================================
// Interrupt-Routine für den Inkrementalgeber (Schritt-Zähler Stellmotor)
void IRAM_ATTR encoderISR()
{
  gpioData.isrEncoderCount++;
}

// ==============================================================================
// Interrupt-Routine für den Referenz-Endschalter (Nullpunkt Stellmotor)
void IRAM_ATTR referenceISR()
{
  gpioData.isrEncoderCount = 0; // Sofortige Kalibrierung der Motor-Ist-Position
}

// ==============================================================================
void getGPIOInputs()
{
  uint32_t nowMs = millis();
  uint32_t nowMicros = micros();

  // --- 1. PHYSALISCHE LIVE-ZUSTÄNDE DER HARDWARE EINLESEN ---
  // Reed-Sensor, Inkrementalgeber und Endschalter direkt von den Pins abfragen
  gpioData.cadenceRawPulse = (digitalRead(KADENCE_PIN) == LOW);
  gpioData.encoderRawPulse = (digitalRead(INKREMENTAL_PIN) == LOW);
  gpioData.referenceSwitchPressed = (digitalRead(REFERENCE_PIN) == LOW);
  gpioData.plusBtnPressed = (digitalRead(GEAR_PLUS) == LOW);
  gpioData.minusBtnPressed = (digitalRead(GEAR_MINUS) == LOW);

  // --- 2. KADENZ BERECHNEN ---
  if (now - gpioData.isrLastCadenceTime > 3000000)
    {
      systemData.cadenceRPM = 0;
    }
  else if (gpioData.isrCadenceDeltaUs > 0)
    {
      systemData.cadenceRPM = 60000000 / isrCadenceDeltaUs;
    }

  // Pedal-Impuls-Flanke aus der ISR an den Kern übergeben
  gpioData.pedalPulseDetected = isrPedalDetected;
  gpioData.isrPedalDetected = false;

  // --- 3. GANGSCHALTUNG (Prellfrei) ---
  if (nowMs - gpioData.lastGearChangeTime > DEBOUNCE_DELAY)
    {
      if (gpioData.plusBtnPressed)
        {
          if (systemData.currentGear < GEAR_COUNT)
            {
              systemData.currentGear++;
              Serial.printf("-> Hochschalten! Neuer Gang: %d\n", systemData.currentGear);
              gpioData.lastGearChangeTime = nowMs;
            }
        }
      else if (gpioData.minusBtnPressed)
        {
          if (systemData.currentGear > 1)
            {
              systemData.currentGear--;
              Serial.printf("-> Runterschalten! Neuer Gang: %d\n", systemData.currentGear);
              gpioData.lastGearChangeTime = nowMs;
            }
        }
    }

  // --- 4. IST-MOTORPOSITION SPEICHERN ---
  gpioData.actualMotorPosition = (int32_t)isrEncoderCount;
}
