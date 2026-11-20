#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <ArduinoOTA.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

// Deine WLAN-Zugangsdaten (Trage hier morgen deine echten Daten ein)
const char *ssid = "DEIN_WLAN_NAME";
const char *password = "DEIN_WLAN_PASSWORT";

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

void WLAN_init()
{
  Serial.println("--- [2] WLAN-Verbindung gestartet ---");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wir warten im Setup maximal 10 Sekunden auf das WLAN,
  // damit das Ergometer zur Not auch ohne WLAN normal startet.
  uint32_t attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20)
    {
      delay(500);
      Serial.print(".");
      attempt++;
    }

  if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n-> WLAN erfolgreich verbunden!");
      Serial.print("-> IP-Adresse: ");
      Serial.println(WiFi.localIP());

      // ==========================================
      // ARDUINO OTA (Over-the-Air) INITIALISIERUNG
      // ==========================================
      ArduinoOTA.setHostname(
          "esp32-ergometer"); // Name, der im VS Code Port-Menü erscheint
      ArduinoOTA.setPassword(
          "ergometer123"); // Optionales Passwort zur Sicherheit beim Flashen

      ArduinoOTA.onStart(
          []() { Serial.println("Start des kabellosen Updates..."); });

      ArduinoOTA.onEnd([]() {
        Serial.println("\nUpdate erfolgreich beendet! ESP32 startet neu...");
      });

      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Fortschritt: %u%%\r", (progress / (total / 100)));
      });

      ArduinoOTA.onError(
          [](ota_error_t error) { Serial.printf("Fehler [%u]: ", error); });

      ArduinoOTA.begin();
      Serial.println("-> OTA-Funk-Updates sind bereit!");
    }
  else
    {
      Serial.println("\n-> WLAN-Timeout! Starte Ergometer im Offline-Modus.");
    }
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
