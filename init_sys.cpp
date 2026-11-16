#include "config.h"
#include "modules.h"
#include "system_data.h"

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

// Initialisierung des Displays
void initOnboardDisplay()
{
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON,
               HIGH); // Stromversorgung für das Display einschalten

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH); // Hintergrundbeleuchtung einschalten

  tft.init();
  tft.setRotation(3);        // Querformat (Landscape 270 Grad)
  tft.fillScreen(TFT_BLACK); // Bildschirm leeren

  Serial.println("-> [2] Onboard-Display initialisiert.");
}

void WLAN_init() { Serial.println("-> [3] WLAN bereit."); }

void Bluetooth_init() { Serial.println("-> [4] Bluetooth FTMS bereit."); }
