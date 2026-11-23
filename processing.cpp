#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

void calcCtrlMode()
{
  uint32_t nowMs = millis();

  // --- FALL A: App (Kinomap) ist aktiv verbunden ---
  if (systemData.btClientConnected)
    {
      if (rxControlMode == 1)
        {
          systemData.controlMode = OpMode::SIMU;
        }
      else
        {
          systemData.controlMode = OpMode::MANUAL;
        }
    }
  // --- FALL B: Pulsgurt ist gekoppelt -> Puls-Training ---
  else if (systemData.hrSensorConnected)
    {
      systemData.controlMode = OpMode::PULS;

      // Regelungsentscheidung exakt alle 10 Sekunden ausführen
      if (nowMs - systemData.lastPulsRegulatorTime >= REGULATOR_INTERVAL_MS)
        {
          systemData.lastPulsRegulatorTime = nowMs;

          // Wir nutzen direkt das stabil gefilterte Signal aus dem Input-Modul!
          int32_t stableHR = systemData.heartRateBPM_Average;

          if (stableHR > 0)
            {
              // Puls zu niedrig ➔ Gang hochschalten für mehr Last
              if (stableHR <= (CARDIO_TARGET_HR - 10))
                {
                  if (systemData.currentGear < GEAR_COUNT)
                    {
                      systemData.currentGear++;
                      Serial.printf("[CARDIO] Puls stabil niedrig (%d bpm). Schalte HOCH -> Gang %d\n", stableHR, systemData.currentGear);
                    }
                }
              // Puls zu hoch ➔ Gang runterschalten für weniger Last
              else if (stableHR >= (CARDIO_TARGET_HR + 10))
                {
                  if (systemData.currentGear > 1)
                    {
                      systemData.currentGear--;
                      Serial.printf("[CARDIO] Puls stabil hoch (%d bpm). Schalte RUNTER -> Gang %d\n", stableHR, systemData.currentGear);
                    }
                }
            }
        }
    }
  // --- FALL C: Manueller Betrieb (Keine Sensoren) ---
  else
    {
      systemData.controlMode = OpMode::MANUAL;
    }
}

// ==============================================================================
// 2. PHYSIKALISCHE GANG- & BREMSMOMENTENBERECHNUNG (calcGear)
// ==============================================================================
void calcGear()
{
  if (systemData.currentGear < 1)
    systemData.currentGear = 1;
  if (systemData.currentGear > GEAR_COUNT)
    systemData.currentGear = GEAR_COUNT;

  float activeRatio = GEAR_RATIOS[systemData.currentGear - 1];
  float wheelCircumferenceMeters = WHEEL_CIRCUMFERENCE_MM / 1000.0f;

  // 1. Virtuelle Geschwindigkeit berechnen
  if (systemData.cadenceRPM > 0)
    {
      systemData.speedKmH = (float)systemData.cadenceRPM * activeRatio * wheelCircumferenceMeters * 0.06f;
    }
  else
    {
      systemData.speedKmH = 0.0f;
    }

  // 2. Millimetergenaue Strecke aufaddieren
  if (systemData.pedalPulseDetected)
    {
      float mmThisRotation = activeRatio * WHEEL_CIRCUMFERENCE_MM;
      systemData.distanceMillimeters += (uint32_t)round(mmThisRotation);
    }

  // 3. Physikalische Leistungsberechnung am Berg & Drehmomente
  if (systemData.cadenceRPM > 0 && systemData.speedKmH > 0.0f)
    {
      float v_ms = systemData.speedKmH / 3.6f;
      float total_power = 0.0f;

      if (systemData.controlMode == OpMode::SIMU)
        {
          float slope_decimal = systemData.targetSlope / 100.0f;

          float p_gravity = TOTAL_SYSTEM_WEIGHT_KG * GRAVITY_G * slope_decimal * v_ms;
          float p_roll = TOTAL_SYSTEM_WEIGHT_KG * GRAVITY_G * CR_ROLL_RESIST * v_ms;
          float p_aero = CW_AERO * (v_ms * v_ms * v_ms);

          total_power = p_gravity + p_roll + p_aero;

          if (total_power < 50.0f)
            total_power = 50.0f;
          if (total_power > 450.0f)
            total_power = 450.0f;
          systemData.targetPowerWatt = (int32_t)round(total_power);
        }
      else
        {
          systemData.targetPowerWatt = 50 + ((systemData.currentGear - 1) * 25);
          total_power = (float)systemData.targetPowerWatt;
        }

      // Drehmoment an der Kurbel (M = P * 60 / (2 * pi * RPM))
      systemData.targetTorqueNm = (total_power * 60.0f) / (2.0f * PI * (float)systemData.cadenceRPM);
    }
  else
    {
      systemData.targetPowerWatt = 0;
      systemData.targetTorqueNm = 0.0f;
    }

  if (systemData.targetTorqueNm > 80.0f)
    {
      systemData.targetTorqueNm = 80.0f;
    }
}

// ==============================================================================
// 3. MOTORPOSITIONIERUNGS-BERECHNUNG (calcMotorPos)
// ==============================================================================
void calcMotorPos()
// Temporärer Platzhalter: Lineares Mapping auf deine 24 Motorstufen (MOTOR_ABSPOS)
// Wir nehmen an, dass 0 Nm = Stufe 1 (0 Schritte) und 80 Nm = Stufe 24 (680 Schritte) entspricht.
{
  float normalizedTorque = systemData.targetTorqueNm / 80.0f;
  int32_t targetLevel = (int32_t)round(normalizedTorque * (float)(MOTOR_POS_COUNT - 1)) + 1;

  if (targetLevel < 1)
    targetLevel = 1;
  if (targetLevel > MOTOR_POS_COUNT)
    targetLevel = MOTOR_POS_COUNT;

  systemData.targetMotorPosition = MOTOR_ABSPOS[targetLevel - 1];
}