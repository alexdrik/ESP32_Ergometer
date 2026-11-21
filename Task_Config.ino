#include "modules.h"

/*
Get-ChildItem -Recurse -Include *.cpp, *.h, *.ino | ForEach-Object {Write-Output "==============================================================================`nFILE: $($_.FullName)`n==============================================================================`n"; Get-Content $_.FullName;Write-Output "`n`n" } | Out-File -FilePath "Gesamter_Code.txt" -Encoding utf8
Get-Content Gesamter_Code.txt | Set-Clipboard

/*
 * ==============================================================================
 *                         PROGRAMMAUFRUF-HIERARCHIE
 * ==============================================================================
 * [Core 0] - System, Netzwerk & Connectivity (Hintergrund-Tasks)
 *   +-- WLAN_init()                (WLAN-Verbindung herstellen & OTA initialisieren)
 *   +-- Bluetooth_init()           (BLE-Server starten & FTMS-Dienste bereitstellen)
 *   +-- WlanUpdateLoop()           (Eigener Task auf Core 0, Intervall: 100ms)
 *         +-- WLAN_Update()        (Lauscht auf Core 0 völlig autark nach OTA-Updates)
 * ------------------------------------------------------------------------------
 * [Core 1] - Applikation & Hardware-Steuerung (Echtzeit-Haupt-Tasks)
 *   +-- setup()                    (Einstiegspunkt für Core 1)
 *   |     +-- ESP_init()           (Basis-GPIOs konfigurieren & Variablen nullen)
 *   |     +-- initOnboardDisplay() (Display-Hardware starten & Gitter leeren)
 *   |     +-- xTaskCreatePinned... (ErgometerTask erstellen und an Core 1 binden)
 *   |     +-- xTaskCreatePinned... (WlanUpdateTask erstellen und an Core 0 binden)
 *   +-- loop()                     (Leerlauf-Sicherungs-Task / Schläft für Watchdog)
 *   +-- ErgometerLoop()            (Hauptschleife auf Core 1, Intervall: 10ms)
 *         +-- main_prg()           (Das zentrale Steuerungsprogramm)
 *               |-- [INPUTS EINLESEN]
 *               |     +-- getFTMSInputs()   (Vorgaben von Bluetooth Fitness-Apps holen)
 *               |     +-- getPulsInputs()   (Pulsdaten von Sensor/Brustgurt lesen)
 *               |     +-- getGPIOInputs()   (Physische Taster & Gangschaltung einlesen)
 *               |-- [VERARBEITUNG & BERECHNUNG]
 *               |     +-- calcCtrlMode()    (Regelungsmodus bestimmen: Watt/Manuell)
 *               |     +-- calcGear()        (Virtuellen Gang berechnen)
 *               |     +-- calcMotorPos()    (Stellmotor-Sollposition ermitteln)
 *               |-- [OUTPUTS SCHREIBEN]
 *               |     +-- setGPIOOutputs()  (Stellmotor für den Widerstand ansteuern)
 *               |     +-- setFTMSOutputs()  (Trainingsdaten über Bluetooth senden)
 *               |-- [DIAGNOSE]
 *                     +-- showDiagData()    (Echtzeit-Matrix & TFT-Display aktualisieren)
 * ==============================================================================
 */

TaskHandle_t ErgometerTask;
TaskHandle_t WlanUpdateTask;

void setup()
{
  ESP_init();
  Display_init();
  WLAN_init();
  Bluetooth_init();

  // Ergometer an Core 1 pinnen
  xTaskCreatePinnedToCore(ErgometerLoop, "ErgometerTask", 10000, NULL, 1,
                          &ErgometerTask, 1);

  // WLAN-Update an Core 0 pinnen (Priorität 1, Core 0)
  xTaskCreatePinnedToCore(WlanUpdateLoop, "WlanUpdateTask", 4000, NULL, 1,
                          &WlanUpdateTask, 0);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }

// Task 1: Läuft auf Core 1 (Reiner Ergometer-Kram)
void ErgometerLoop(void *pvParameters)
{
  Serial.print("Ergometer-Task gestartet auf Core: ");
  Serial.println(xPortGetCoreID());

  for (;;)
    {
      main_prg();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task 2: Läuft im Hintergrund auf Core 0 (Nur WLAN-Updates)
void WlanUpdateLoop(void *pvParameters)
{
  Serial.print("WLAN-Update-Task gestartet auf Core: ");
  Serial.println(xPortGetCoreID());
  for (;;)
    {
      WLAN_Update(); // Lauscht auf Core 0 nach Updates
      vTaskDelay(pdMS_TO_TICKS(
          100)); // Muss nicht so oft laufen (100ms reicht völlig!)
    }
}

// Deklaration der internen Funktion aus init_sys.cpp
void handleHeartRateConnection();

void WlanUpdateLoop(void *pvParameters)
{
  Serial.print("WLAN & BLE Client-Task gestartet auf Core: ");
  Serial.println(xPortGetCoreID());
  for (;;)
    {
      WLAN_Update();                  // Lauscht auf Core 0 nach OTA-Updates
      handleHeartRateConnection();    // Behandelt Pulsgurt-Verbindungen auf Core 0
      vTaskDelay(pdMS_TO_TICKS(100)); // 100ms CPU-Entlastung
    }
}