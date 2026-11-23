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

static BLEAdvertisedDevice *myDevice;
static BLEClient *pClient = nullptr;
static bool doConnect = false;

static void hrNotifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
  if (length >= 2)
    {
      uint8_t flags = pData[0];
      int32_t rawHeartRate = 0;

      if ((flags & 0x01) == 0)
        {
          rawHeartRate = pData[1];
        }
      else
        {
          rawHeartRate = pData[1] | (pData[2] << 8);
        }

      // --- DER REKURSIVE FILTER ---
      if (rawHeartRate > 0)
        {
          // 1. Letzten (ältesten) Wert an der Zeigerposition abziehen
          pulsGurtData.hrSum -= pulsGurtData.hrBuffer[pulsGurtData.bufferIndex];

          // 2. Neuen Wert in den Puffer schreiben
          pulsGurtData.hrBuffer[pulsGurtData.bufferIndex] = rawHeartRate;

          // 3. Neuen Wert auf die Summe aufaddieren
          pulsGurtData.hrSum += rawHeartRate;

          // 4. Index für den nächsten Wert verschieben (mit Ringüberlauf)
          pulsGurtData.bufferIndex++;
          if (pulsGurtData.bufferIndex >= HR_BUFFER_SIZE)
            {
              pulsGurtData.bufferIndex = 0;
            }

          // 5. Mittelwert berechnen und an Core 1 übergeben
          pulsGurtData.heartRateBPM = rawHeartRate;
          pulsGurtData.heartRateAverage = pulsGurtData.hrSum / HR_BUFFER_SIZE;
        }
    }
}

// Callback-Klasse für die Geräte-Suche (Scan)
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(hrServiceUUID))
      {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        Serial.println("-> Pulsgurt in Reichweite gefunden!");
      }
  }
};

// Logik für den Verbindungsaufbau zum Pulsgurt
bool connectToHeartRateSensor()
{
  Serial.println("-> Verbinde mit Pulsgurt...");
  pClient = BLEDevice::createClient();

  if (!pClient->connect(myDevice))
    {
      return false;
    }

  // 1. Herzfrequenz-Dienst abonnieren
  BLERemoteService *pRemoteService = pClient->getService(hrServiceUUID);
  if (pRemoteService != nullptr)
    {
      BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getRemoteCharacteristic(hrCharUUID);
      if (pRemoteCharacteristic != nullptr && pRemoteCharacteristic->canNotify())
        {
          pRemoteCharacteristic->registerForNotify(hrNotifyCallback);
          rxHrConnected = true;
          Serial.println("-> Puls-Kanal erfolgreich abonniert.");
        }
    }

  // 2. Batterieladezustand auslesen
  BLERemoteService *pBatService = pClient->getService(batServiceUUID);
  if (pBatService != nullptr)
    {
      BLERemoteCharacteristic *pBatCharacteristic = pBatService->getRemoteCharacteristic(batCharUUID);
      if (pBatCharacteristic != nullptr && pBatCharacteristic->canRead())
        {
          std::string val = pBatCharacteristic->readValue();
          if (val.length() > 0)
            {
              rxHrBattery = (int32_t)val[0];
              Serial.printf("-> Pulsgurt-Batterie: %d %%\n", rxHrBattery);
            }
        }
    }

  return rxHrConnected;
}

// Diese Funktion rufen wir in deinem WLAN/Bluetooth-Task auf Core 0 auf!
void handleHeartRateConnection()
{
  if (doConnect)
    {
      if (connectToHeartRateSensor())
        {
          Serial.println("-> Pulsgurt erfolgreich gekoppelt!");
        }
      else
        {
          Serial.println("-> Verbindungsaufbau fehlgeschlagen. Starte neuen Suchlauf...");
          BLEDevice::getScan()->start(5, false);
        }
      doConnect = false;
    }

  // Falls die Verbindung abbricht, automatisch den Status zurücksetzen und neu suchen
  if (pClient != nullptr && !pClient->isConnected() && rxHrConnected)
    {
      rxHrConnected = false;
      rxHeartRate = 0;
      Serial.println("-> Verbindung zum Pulsgurt verloren. Suche erneut...");
      BLEDevice::getScan()->start(5, false);
    }
}

// Wird am Ende deines Bluetooth_init() in init_sys.cpp aufgerufen
void initHeartRateScan()
{
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // Suche für 5 Sekunden
  Serial.println("-> BLE-Suchlauf nach Pulsgurt gestartet...");
}

void ESP_init()
{
  Serial.begin(115200);

  // 1. GPIO Pins konfigurieren (Mit internen Pull-Ups)
  pinMode(KADENCE_PIN, INPUT_PULLUP);
  pinMode(INKREMENTAL_PIN, INPUT_PULLUP);
  pinMode(REFERENCE_PIN, INPUT_PULLUP);

  // Pins für Gangschaltung und Menü
  pinMode(GEAR_PLUS, INPUT_PULLUP);
  pinMode(GEAR_MINUS, INPUT_PULLUP);
  pinMode(MENU_SWITCH, INPUT_PULLUP);
  pinMode(MENU_EDIT, INPUT_PULLUP);

  // DRV8833 PWM Pins an die Hardware-Kanäle koppeln
  ledcSetup(CH_MOTOR_POS, PWM_FREQ, PWM_RES_BITS);
  ledcSetup(CH_MOTOR_NEG, PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(MOTOR_POS_PIN, CH_MOTOR_POS);
  ledcAttachPin(MOTOR_NEG_PIN, CH_MOTOR_NEG);

  // 2. Hardware-Interrupts aktivieren
  attachInterrupt(digitalPinToInterrupt(KADENCE_PIN), cadenceISR, RISING);
  attachInterrupt(digitalPinToInterrupt(INKREMENTAL_PIN), encoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(REFERENCE_PIN), referenceISR, RISING);

  // 3. Systemdaten-Modell initialisieren
  systemData.cadenceRPM = 0;
  systemData.heartRateBPM = 0;
  systemData.heartRateBatteryPercent = 100;
  systemData.pedalPulseDetected = false;
  systemData.currentGear = 4;
  systemData.targetMotorPosition = 0;
  systemData.controlMode = OpMode::HOMING;
  systemData.speedKmH = 0.0f;
  systemData.targetSlope = 0.0f;
  systemData.currentPowerWatt = 0;
  systemData.hrSensorConnected = false;
  systemData.btClientConnected = false;

  // --- 4. PULSGURT MIT 90 BPM INITIALISIEREN ---
  pulsGurtData.hrSensorConnected = false;
  pulsGurtData.heartRateBPM = 90;
  pulsGurtData.heartRateAverage = 90;
  pulsGurtData.batteryPercent = 100;
  pulsGurtData.bufferIndex = 0;
  pulsGurtData.hrSum = 900; // 10 Einträge * 90 bpm
  for (int32_t i = 0; i < HR_BUFFER_SIZE; i++)
    {
      pulsGurtData.hrBuffer[i] = 90;
    }

  // --- 5. MOTORRAMPEN INITIALISIEREN ---
  gpioData.currentMotorState = MotorState::STOPPED;
  gpioData.motorStateStartTime = 0;
  gpioData.activeDirectionPos = true;)

  Serial.println("-> [1] ESP32-Basis & Interrupt-GPIOs erfolgreich konfiguriert.");
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

  // Gitter des neuen displayData-Structs beim Start leeren
  for (uint32_t r = 0; r < MATRIX_ROWS; r++)
    {
      for (uint32_t c = 0; c < MATRIX_COLS; c++)
        {
          strcpy(displayData.displayLabels[r][c], "");
          displayData.displayValues[r][c] = 0;
          strcpy(displayData.displayMatrix[r][c], "");
        }
    }
  Serial.println("-> [2] Display initialisiert.");
}

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

  initHeartRateScan();

  // Bike Data Charakteristik erstellen (Notify ist Pflicht, damit die App Daten empfangen kann)
  pBikeDataCharacteristic = pService->createCharacteristic(
      FTMS_BIKE_DATA_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
  pBikeDataCharacteristic->addDescriptor(new BLE2902());

  Serial.println("-> Bluetooth FTMS aktiv auf Core 0.");
}
