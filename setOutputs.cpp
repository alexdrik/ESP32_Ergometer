#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

extern volatile uint32_t isrEncoderCount;

// PWM Kanal-Zuweisung
const uint8_t CH_MOTOR_POS = 0;
const uint8_t CH_MOTOR_NEG = 1;

// Motor-Zustände für die Rampe
enum class MotorState { STOPPED,
                        ACCEL,
                        RUNNING,
                        DECEL };
static MotorState currentMotorState = MotorState::STOPPED;
static uint32_t motorStateStartTime = 0;
static bool activeDirectionPos = true; // true = Vorwärts, false = Rückwärts

// Hilfsfunktion: Setzt die PWM-Geschwindigkeit mit einer linearen 100ms-Rampe
void driveMotorPWM(bool forward, bool stopRequest)
{
  uint32_t elapsed = millis() - motorStateStartTime;
  uint32_t duty = 0;

  if (stopRequest)
    {
      if (currentMotorState != MotorState::DECEL && currentMotorState != MotorState::STOPPED)
        {
          currentMotorState = MotorState::DECEL;
          motorStateStartTime = millis();
          elapsed = 0;
        }

      if (currentMotorState == MotorState::DECEL)
        {
          if (elapsed < 100)
            {
              duty = 255 - (elapsed * 255 / 100); // Bremse von 100% auf 0%
            }
          else
            {
              duty = 0;
              currentMotorState = MotorState::STOPPED;
            }
        }
    }
  else
    {
      activeDirectionPos = forward;

      if (currentMotorState == MotorState::STOPPED)
        {
          currentMotorState = MotorState::ACCEL;
          motorStateStartTime = millis();
          elapsed = 0;
        }

      if (currentMotorState == MotorState::ACCEL)
        {
          if (elapsed < 100)
            {
              duty = (elapsed * 255 / 100); // Sanft beschleunigen von 0% auf 100%
            }
          else
            {
              duty = 255;
              currentMotorState = MotorState::RUNNING;
            }
        }
      else if (currentMotorState == MotorState::RUNNING)
        {
          duty = 255; // Volle Kraft
        }
    }

  // PWM-Signale an die DRV8833-Kanäle senden
  if (activeDirectionPos)
    {
      ledcWrite(CH_MOTOR_NEG, 0);
      ledcWrite(CH_MOTOR_POS, duty);
    }
  else
    {
      ledcWrite(CH_MOTOR_POS, 0);
      ledcWrite(CH_MOTOR_NEG, duty);
    }
}

// Die verfeinerte, präzise Motor-Ablaufsteuerung
static bool isBacklashCorrectionActive = false;
static int32_t backlashTargetPos = 0;

void setGPIOOutputs()
{
  systemData.actualMotorPosition = (int32_t)isrEncoderCount;

  // --- FALL 1: Referenzfahrt (Homing) nach dem Start ---
  if (systemData.controlMode == OpMode::HOMING)
    {
      if (digitalRead(REFERENCE_PIN) == HIGH)
        {
          driveMotorPWM(false, false); // Sanft rückwärts fahren
        }
      else
        {
          driveMotorPWM(false, true); // Sanft stoppen

          if (currentMotorState == MotorState::STOPPED)
            {
              isrEncoderCount = 0;
              systemData.actualMotorPosition = 0;
              systemData.controlMode = OpMode::MANUAL; // Wechsle in den manuellen Modus!
              systemData.currentGear = 4;              // Startgang 4 setzen
              Serial.println("-> Homing abgeschlossen. Sanfter Übergang zu MANUAL.");
            }
        }
      return;
    }

  // --- FALL 2: Regulärer Betrieb mit Getriebespiel-Korrektur ---
  int32_t finalTarget = systemData.targetMotorPosition;
  if (finalTarget < MOTOR_ABSPOS[0])
    finalTarget = MOTOR_ABSPOS[0];
  if (finalTarget > MOTOR_ABSPOS[MOTOR_POS_COUNT - 1])
    finalTarget = MOTOR_ABSPOS[MOTOR_POS_COUNT - 1];

  // Getriebespiel-Korrektur aktiv (Rückwärtsüberlauf)
  if (isBacklashCorrectionActive)
    {
      if (systemData.actualMotorPosition > backlashTargetPos)
        {
          driveMotorPWM(false, false); // Sanft rückwärts fahren
        }
      else
        {
          driveMotorPWM(false, true); // Sanft abbremsen

          if (currentMotorState == MotorState::STOPPED)
            {
              isBacklashCorrectionActive = false;
            }
        }
      return;
    }

  int32_t positionError = finalTarget - systemData.actualMotorPosition;

  if (positionError > 0)
    {
      driveMotorPWM(true, false); // Sanft vorwärts anfahren
    }
  else if (positionError < 0)
    {
      backlashTargetPos = finalTarget - (int32_t)MOTOR_NEG_RELPOS;
      if (backlashTargetPos < 0)
        backlashTargetPos = 0;

      isBacklashCorrectionActive = true;
      driveMotorPWM(false, true); // Vorherige Bewegung abbremsen
      Serial.printf("[MOTOR] Getriebespiel-Korrektur gestartet! Ziel: %d\n", backlashTargetPos);
    }
  else
    {
      driveMotorPWM(true, true); // Zielpunkt exakt erreicht -> Sanft abbremsen und halten
    }
}

void setFTMSOutputs()
{
  // Bluetooth-Daten senden folgt
}
