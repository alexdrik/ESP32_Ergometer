#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <Arduino.h>

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
  int32_t actualPos = gpioData.actualMotorPosition;
  int32_t finalTarget = gpioData.targetMotorPosition;

  // Absicherung des Zielbereichs
  if (finalTarget < MOTOR_ABSPOS[0])
    finalTarget = MOTOR_ABSPOS[0];
  if (finalTarget > MOTOR_ABSPOS[MOTOR_POS_COUNT - 1])
    finalTarget = MOTOR_ABSPOS[MOTOR_POS_COUNT - 1];

  // --- FALL 1: Referenzfahrt (Homing) ---
  if (systemData.controlMode == OpMode::HOMING)
    {
      if (gpioData.referenceSwitchPressed == false)
        {
          driveMotorPWM(false, false); // Sanft rückwärts suchen
        }
      else
        {
          driveMotorPWM(false, true); // Sanft abbremsen
          if (gpioData.currentMotorState == MotorState::STOPPED)
            {
              systemData.controlMode = OpMode::MANUAL;
              systemData.currentGear = 4;
              Serial.println("-> Homing erfolgreich abgeschlossen!");
            }
        }
      return;
    }

  // --- FALL 2: Regulärer Betrieb (Zustandsmaschine) ---

  // Zustand A: Getriebespiel-Korrektur läuft aktiv im Hintergrund
  if (gpioData.currentMotorState == MotorState::BACKLASH)
    {
      if (actualPos > backlashTargetPos)
        {
          driveMotorPWM(false, false); // Fahre aktiv weiter rückwärts
        }
      else
        {
          driveMotorPWM(false, true); // Ziel-Überlauf erreicht -> sanft abbremsen

          if (gpioData.currentMotorState == MotorState::STOPPED)
            {
              // Rückwärts-Überlauf beendet!
              // Jetzt schalten wir den Zustand wieder frei, damit der Motor regulär
              // und spielfrei von hinten (in positive Richtung) anfahren kann.
              Serial.println("[MOTOR] Rückwärts-Überlauf beendet. Fahre jetzt spielfrei vorwärts an.");
            }
        }
      return;
    }

  // Zustand B: Berechne den aktuellen Positionsfehler
  int32_t positionError = finalTarget - actualPos;

  if (positionError > 0)
    {
      // Vorwärtsfahrt (schwerer): Direkt und ohne Umwege anfahren
      driveMotorPWM(true, false);
    }
  else if (positionError < 0)
    {
      // Rückwärtsfahrt (leichter): Getriebespiel-Kompensation einleiten!
      // Berechne die temporäre Überlauf-Position (Sollwert minus 32 Schritte)
      backlashTargetPos = finalTarget - (int32_t)MOTOR_NEG_RELPOS;
      if (backlashTargetPos < 0)
        backlashTargetPos = 0;

      // Wir wechseln aktiv in den BACKLASH-Zustand und bremsen die aktuelle Bewegung ab
      gpioData.currentMotorState = MotorState::BACKLASH;
      driveMotorPWM(false, true);
      Serial.printf("[MOTOR] Getriebespiel-Zustand aktiv! Fahre überlaufend zurück auf: %d\n", backlashTargetPos);
    }
  else
    {
      // Zielpunkt exakt und spielfrei erreicht -> Sanft abbremsen und Haltestrom halten
      driveMotorPWM(true, true);
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

      uint8_t txBuffer[8];

      // 1. Flags eintragen (Byte 0 & 1)
      uint16_t flags = 0x0024;
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