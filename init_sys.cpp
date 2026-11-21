#include "config.h"
#include "modules.h"
#include "secrets.h"
#include "system_data.h"
#include <ArduinoOTA.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

// Definition der globalen Datenstruktur (nur einmal im Projekt!)
ErgometerData systemData;

// geheime Zugangsdatena aus secrets.h reinkopieren
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASSWORD;

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

void WLAN_init() { Serial.println("-> [3] WLAN nicht bereit."); }

// Asynchrone Zwischenspeicher für den Core-Übergang (Core 0 -> Core 1)
volatile uint32_t rxControlMode = 0;
volatile uint32_t rxTargetPower = 0;
volatile float rxTargetSlope = 0.0f;
volatile bool rxBtConnected = false;

// UUIDs für den Fitness Machine Service (FTMS)
#define FTMS_SERVICE_UUID "00001826-0000-1000-8000-00805f9b34fb"
#define FTMS_CONTROL_POINT_UUID "00002ad9-0000-1000-8000-00805f9b34fb"

BLEServer *pServer = nullptr;
BLECharacteristic *pControlPointCharacteristic = nullptr;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    rxBtConnected = true;
  }

  void onDisconnect(BLEServer *pServer)
  {
    rxBtConnected = false;
    rxControlMode = 0; // Bei Verbindungsabbruch zurück in den manuellen Modus
    pServer->startAdvertising();
  }
};

class ControlPointCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
      {
        uint8_t opcode = value[0];

        // Opcode 0x00: Request Control (Kinomap fordert die Kontrolle an)
        if (opcode == 0x00)
          {
            uint8_t response[] = {0x80, 0x00, 0x01};
            pControlPointCharacteristic->setValue(response, 3);
            pControlPointCharacteristic->indicate();
          }

        // Opcode 0x05: Set Target Power (ERG-Modus / Wattvorgabe)
        else if (opcode == 0x05 && value.length() >= 3)
          {
            rxTargetPower = (uint32_t)(value[1] | (value[2] << 8));
            rxControlMode = 2; // Modus auf Watt-Steuerung setzen

            uint8_t response[] = {0x80, 0x05, 0x01};
            pControlPointCharacteristic->setValue(response, 3);
            pControlPointCharacteristic->indicate();
          }

        // Opcode 0x11: Set Simulation Parameters (Slope / Steuerungsvorgabe)
        else if (opcode == 0x11 && value.length() >= 7)
          {
            // FTMS-Standard: Slope wird als vorzeichenbehafteter int16 übertragen (Auflösung 0.1 %)
            int16_t rawSlope = (int16_t)(value[3] | (value[4] << 8));

            // Umrechnung in echte physikalische float-Prozente (z.B. -15 -> -1.5f)
            rxTargetSlope = (float)rawSlope / 10.0f;
            rxControlMode = 1; // Modus auf Strecken-Simulation setzen

            uint8_t response[] = {0x80, 0x11, 0x01};
            pControlPointCharacteristic->setValue(response, 3);
            pControlPointCharacteristic->indicate();
          }
      }
  }
};

void Bluetooth_init()
{
  BLEDevice::init("DIY-Smartbike");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(FTMS_SERVICE_UUID);

  pControlPointCharacteristic = pService->createCharacteristic(
      FTMS_CONTROL_POINT_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
  pControlPointCharacteristic->setCallbacks(new ControlPointCallbacks());
  pControlPointCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(FTMS_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  pServer->startAdvertising();

  Serial.println("-> Bluetooth FTMS aktiv auf Core 0.");
}
