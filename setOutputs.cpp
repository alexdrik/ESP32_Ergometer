#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

extern volatile uint32_t isrEncoderCount;
extern BLECharacteristic *pBikeDataCharacteristic;

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
  // Wir senden nur Daten, wenn ein Smartphone/Tablet aktiv verbunden ist
  if (!systemData.btClientConnected || pBikeDataCharacteristic == nullptr)
    {
      return;
    }

  static uint32_t lastTxTime = 0;
  if (millis() - lastTxTime >= 1000) // Exakt jede Sekunde senden
    {
      lastTxTime = millis();

      // FTMS Indoor Bike Data Format (Standard-Definition):
      // Byte 0-1: Flags (bestimmen, welche Daten gesendet werden)
      //   Bit 0: 0 = More Data present (wir senden alles in einem Paket)
      //   Bit 1: 1 = Average Speed present (nein, wir senden nur die aktuelle)
      //   Bit 2: 1 = Instantaneous Cadence present (Ja! Trittfrequenz ist dabei)
      //   Bit 3: 1 = Total Distance present (optional)
      //   Bit 4: 1 = Resistance Level present (optional)
      //   Bit 5: 1 = Instantaneous Power present (Ja! Leistung ist dabei)
      // Flags-Wert: Bit 2 (Cadence) und Bit 5 (Power) aktiv -> 0x0004 | 0x0020 = 0x0024
      // Da wir die Geschwindigkeit immer mitsenden (ist Standard, wenn Flags Bit 0 = 0):
      uint16_t flags = 0x0024;

      uint8_t txBuffer[8];

      // 1. Flags eintragen (Byte 0 & 1)
      txBuffer[0] = (uint8_t)(flags & 0xFF);
      txBuffer[1] = (uint8_t)((flags >> 8) & 0xFF);

      // 2. Geschwindigkeit eintragen (Byte 2 & 3)
      // FTMS-Standard verlangt km/h mit einer Auflösung von 0.01 (km/h * 100)
      uint16_t speedVal = (uint16_t)(systemData.speedKmH * 100.0f);
      txBuffer[2] = (uint8_t)(speedVal & 0xFF);
      txBuffer[3] = (uint8_t)((speedVal >> 8) & 0xFF);

      // 3. Trittfrequenz eintragen (Byte 4 & 5)
      // FTMS-Standard verlangt Kurbelumdrehungen mit einer Auflösung von 0.5 (RPM * 2)
      uint16_t cadenceVal = (uint16_t)(systemData.cadenceRPM * 2);
      txBuffer[4] = (uint8_t)(cadenceVal & 0xFF);
      txBuffer[5] = (uint8_t)((cadenceVal >> 8) & 0xFF);

      // 4. Leistung eintragen (Byte 6 & 7)
      // FTMS-Standard verlangt Watt als vorzeichenbehafteten 16-Bit Wert (Direktwert)
      int16_t powerVal = (int16_t)systemData.targetPowerWatt; // Oder Ist-Watt, sobald berechnet
      txBuffer[6] = (uint8_t)(powerVal & 0xFF);
      txBuffer[7] = (uint8_t)((powerVal >> 8) & 0xFF);

      // Daten an den Bluetooth-Stack übergeben und an Kinomap abschicken!
      pBikeDataCharacteristic->setValue(txBuffer, 8);
      pBikeDataCharacteristic->notify(); // Löst das Senden auf Core 0 aus!
    }
}