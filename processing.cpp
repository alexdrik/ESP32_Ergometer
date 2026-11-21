#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

// Konstanten und statische Variablen für die Pulsregelung
const int32_t CARDIO_TARGET_HR = 130;         // Soll-Puls: 130 bpm
const uint32_t REGULATOR_INTERVAL_MS = 10000; // Regel-Intervall: 10 Sekunden (10000 ms)

static uint32_t lastRegulatorTime = 0;
static int32_t cardioTargetPower = 100; // Startwert: 100 Watt (entspricht Gang 4)

// Ringpuffer für die Puls-Mittelung über 10 Sekunden (1 Messung pro Sekunde)
const int32_t BUFFER_SIZE = 10;
static int32_t hrBuffer[BUFFER_SIZE] = {0};
static int32_t bufferIndex = 0;
static uint32_t lastBufferTime = 0;
static bool bufferFull = false;

void calcCtrlMode()
{
  uint32_t nowMs = millis();

  // 1. Pulsdaten-Mittelung
  if (systemData.hrSensorConnected && systemData.heartRateBPM > 0)
    {
      if (nowMs - lastBufferTime >= 1000)
        {
          lastBufferTime = nowMs;
          hrBuffer[bufferIndex] = systemData.heartRateBPM;
          bufferIndex++;
          if (bufferIndex >= BUFFER_SIZE)
            {
              bufferIndex = 0;
              bufferFull = true;
            }
        }
    }
  else
    {
      memset(hrBuffer, 0, sizeof(hrBuffer));
      bufferIndex = 0;
      bufferFull = false;
      cardioTargetPower = 100;
    }

  // 2. Ziel-Leistungs-Vorgabe (targetPowerWatt) je nach Modus ermitteln
  if (systemData.btClientConnected)
    {
      // Bei App-Verbindung wird der Modus direkt von FTMS gesteuert:
      // Entweder controlMode 1 (Simulation) oder 2 (Ziel-Watt)
    }
  // --- FALL B: Pulsgurt gekoppelt -> Puls-Steuerung schaltet die Gänge ---
  else if (systemData.hrSensorConnected)
    {
      systemData.controlMode = 3;

      if (bufferFull && (nowMs - lastRegulatorTime >= REGULATOR_INTERVAL_MS))
        {
          lastRegulatorTime = nowMs;

          int32_t hrSum = 0;
          for (int32_t i = 0; i < BUFFER_SIZE; i++)
            hrSum += hrBuffer[i];
          int32_t averageHR = hrSum / BUFFER_SIZE;

          // Puls zu niedrig -> Schalte Gang hoch (mehr Widerstand)
          if (averageHR <= (CARDIO_TARGET_HR - 10))
            {
              if (systemData.currentGear < GEAR_COUNT)
                {
                  systemData.currentGear++;
                  Serial.printf("[CARDIO] Puls zu niedrig (%d bpm). Schalte HOCH in Gang %d\n", averageHR, systemData.currentGear);
                }
            }
          // Puls zu hoch -> Schalte Gang runter (weniger Widerstand)
          else if (averageHR >= (CARDIO_TARGET_HR + 10))
            {
              if (systemData.currentGear > 1)
                {
                  systemData.currentGear--;
                  Serial.printf("[CARDIO] Puls zu hoch (%d bpm). Schalte RUNTER in Gang %d\n", averageHR, systemData.currentGear);
                }
            }
        }
    }

  // --- FALL C: Manueller Modus (Keine App, kein Puls) ---
  else
    {
      systemData.controlMode = 0;
    }
}

void calcGear()
{
  float activeRatio = GEAR_RATIOS[systemData.currentGear - 1];
  float wheelCircumferenceMeters = WHEEL_CIRCUMFERENCE_MM / 1000.0f;

  // ==============================================================================
  // 1. GESCHWINDIGKEIT BERECHNEN (Unter Annahme von Steigung 0% auf flacher Strecke)
  // ==============================================================================
  if (systemData.cadenceRPM > 0)
    {
      systemData.speedKmH = (float)systemData.cadenceRPM * activeRatio * wheelCircumferenceMeters * 0.06f;
    }
  else
    {
      systemData.speedKmH = 0.0f;
    }

  // 2. Millimetergenaue Strecke aufaddieren bei jedem Pedal-Impuls
  if (systemData.pedalPulseDetected)
    {
      float mmThisRotation = activeRatio * WHEEL_CIRCUMFERENCE_MM;
      systemData.distanceMillimeters += (uint32_t)round(mmThisRotation);
    }

  // ==============================================================================
  // 3. PHYSIKALISCHES SULL-DREHMOMENT (Nm) AN DER KURBEL BERECHNEN
  // ==============================================================================
  if (systemData.cadenceRPM > 0 && systemData.speedKmH > 0.0f)
    {
      float v_ms = systemData.speedKmH / 3.6f;

      // Wir holen uns das Steigungsprofil:
      // Im Simulationsmodus (1) die echte targetSlope der App, sonst flache Strecke (0.0%)
      float slope_decimal = (systemData.controlMode == 1) ? (systemData.targetSlope / 100.0f) : 0.0f;

      // Physikalische Leistungsverluste am Berg (Hubleistung, Rollen, Aero)
      float p_gravity = TOTAL_SYSTEM_WEIGHT_KG * GRAVITY_G * slope_decimal * v_ms;
      float p_roll = TOTAL_SYSTEM_WEIGHT_KG * GRAVITY_G * CR_ROLL_RESIST * v_ms;
      float p_aero = CW_AERO * (v_ms * v_ms * v_ms);

      float total_power = p_gravity + p_roll + p_aero;

      // Leistungsgrenzen
      if (total_power < 50.0f)
        total_power = 50.0f;
      if (total_power > 450.0f)
        total_power = 450.0f;

      systemData.targetPowerWatt = (int32_t)round(total_power);

      // Umrechnung von Soll-Leistung (Watt) in physikalisches Soll-Drehmoment (Nm) an der Kurbel:
      // M_soll = P_soll / omega = P_soll * 60 / (2 * pi * RPM)
      systemData.targetTorqueNm = (total_power * 60.0f) / (2.0f * PI * (float)systemData.cadenceRPM);
    }
  else
    {
      systemData.targetPowerWatt = 0;
      systemData.targetTorqueNm = 0.0f;
    }

  // Sicherheitsbegrenzung für das berechnete Soll-Drehmoment an der Kurbel (z.B. max 80 Nm)
  if (systemData.targetTorqueNm > 80.0f)
    {
      systemData.targetTorqueNm = 80.0f;
    }
}

void calcMotorPos()
{
  // ==============================================================================
  // 4. HARDWARE-MAPPER: Drehmoment (targetTorqueNm) -> Motorposition ( targetMotorPosition )
  //    (Diese Zuordnung können wir im nächsten Schritt mit deinen Tabellen füllen)
  // ==============================================================================

  // Temporärer Platzhalter: Lineares Mapping auf deine 24 Motorstufen (MOTOR_ABSPOS)
  // Wir nehmen an, dass 0 Nm = Stufe 1 (0 Schritte) und 80 Nm = Stufe 24 (680 Schritte) entspricht.
  float normalizedTorque = systemData.targetTorqueNm / 80.0f; // Wert von 0.0f bis 1.0f

  int32_t targetLevel = (int32_t)round(normalizedTorque * (float)(MOTOR_POS_COUNT - 1)) + 1;

  if (targetLevel < 1)
    targetLevel = 1;
  if (targetLevel > MOTOR_POS_COUNT)
    targetLevel = MOTOR_POS_COUNT;

  systemData.targetMotorPosition = MOTOR_ABSPOS[targetLevel - 1];
}