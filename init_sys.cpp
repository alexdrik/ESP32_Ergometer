#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <TFT_eSPI.h>

// Definition der globalen Datenstruktur (nur einmal im Projekt!)
ErgometerData systemData;

void ESP_init()
{
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  systemData.cadenceRPM = 0;
  systemData.heartRateBPM = 0;
  systemData.pedalPulseDetected = false;
  systemData.currentGear = 1;
  systemData.targetMotorPosition = 0;
  systemData.controlMode = 0;
  systemData.speedKmH = 0;
  systemData.currentPowerWatt = 0;

  Serial.println("-> [1] ESP32-Basis initialisiert.");
}

void Display_init()
{
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Alles leeren beim Start
  for (uint32_t r = 0; r < MATRIX_ROWS; r++)
    {
      for (uint32_t c = 0; c < MATRIX_COLS; c++)
        {
          strcpy(systemData.displayLabels[r][c], "");
          systemData.displayValues[r][c] = 0;
          strcpy(systemData.displayMatrix[r][c], "");
        }
    }

  Serial.println("-> [2] Display initialisiert.");
}

void WLAN_init() { Serial.println("-> [3] WLAN bereit."); }

void Bluetooth_init() { Serial.println("-> [4] Bluetooth FTMS bereit."); }
