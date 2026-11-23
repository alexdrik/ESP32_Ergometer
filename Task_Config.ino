#include "modules.h"

TaskHandle_t ErgometerTask;
TaskHandle_t WirelessTask; // Umbenannt! (vorher WlanUpdateTask)

// Vorab-Deklarationen für den Compiler
void ErgometerLoop(void *pvParameters);
void wirelessConnectivityLoop(void *pvParameters);
void handleHeartRateConnection();

/*
Get-ChildItem -Recurse -Include *.cpp, *.h, *.ino -Exclude .git*, *.gitignore* | ForEach-Object {Write-Output "==============================================================================`nFILE: $($_.FullName)`n==============================================================================`n"; Get-Content $_.FullName; Write-Output "`n`n" } | Out-File -FilePath "Gesamter_Code.txt" -Encoding utf8
Get-Content Gesamter_Code.txt | Set-Clipboard

todo:
Code umbauen auf: "float calculatePower(float slope, float speed, float weight);"
Spaghetti-Code entflechten und Datenfluß sauberer darstellen
Debugging über Display definieren und ausgeben
serial.print durch WLAN.print austauschen
Aufrufhierarchie mit Doxygen ausprobieren

 * ==============================================================================
 *                         PROGRAMMAUFRUF-HIERARCHIE
 * ==============================================================================
 * [Core 0] - Funkverbindungen, Netzwerk & System (Hintergrund-Tasks)
 *   +-- wirelessConnectivityLoop()         (Eigener Task auf Core 0, Intervall: 100ms)
 *         |-- handleWlanOTA()              (Lauscht auf Core 0 völlig autark nach OTA-Updates)
 *         +-- handleHeartRateConnection()  (Verwaltet die Verbindung zum BLE-Pulsgurt)
 * ------------------------------------------------------------------------------
 * [Core 1] - Applikation & Hardware-Echtzeitsteuerung (Haupt-Tasks)
 *   +-- setup()                             (Einstiegspunkt für Core 1)
 *   |     |-- ESP_init()                    (Basis-GPIOs konfigurieren & Variablen nullen)
 *   |     |-- Display_init()                (Display-Hardware starten & Gitter leeren)
 *   |     |-- WLAN_init()                   (WLAN-Verbindung starten & OTA initialisieren)
 *   |     |-- Bluetooth_init()              (BLE-Server starten, FTMS & HR-Scan aktivieren)
 *   |     |-- xTaskCreatePinned...          (ErgometerTask erstellen und an Core 1 binden)
 *   |     +-- xTaskCreatePinned...          (WirelessTask erstellen und an Core 0 binden)
 *   +-- loop()                              (Leerlauf-Sicherungs-Task / Schläft für Watchdog)
 *   +-- ErgometerLoop()                     (Hauptschleife auf Core 1, Intervall: 10ms)
 *         +-- main_prg()                    (Das zentrale Steuerungsprogramm)
 *               |-- [INPUTS EINLESEN]
 *               |     |-- getFTMSInputs()   (Empfängt Steigungsdaten von Kinomap)
 *               |     |-- getPulsInputs()   (Gefilterten Puls & Batteriewerte einlesen)
 *               |     +-- getGPIOInputs()   (Physische Taster & Ist-Motorposition einlesen)
 *               |-- [VERARBEITUNG & BERECHNUNG]
 *               |     |-- calcCtrlMode()    (Regelungsmodus bestimmen: MANUAL, PULS, SIMU)
 *               |     |-- calcGear()        (Geschwindigkeit, Strecke & Soll-Torque berechnen)
 *               |     +-- calcMotorPos()    (Soll-Drehmoment in Soll-Motorschritte mappen)
 *               |-- [OUTPUTS SCHREIBEN]
 *               |     |-- setGPIOOutputs()  (Stellmotor mit Backlash-Kompensation & Rampen ansteuern)
 *               |     +-- setFTMSOutputs()  (Live-Fahrdaten über Bluetooth an Kinomap senden)
 *               |-- [DIAGNOSE]
 *                     +-- showDiagData()    (Echtzeit-Matrix & TFT-Display aktualisieren)
 * ==============================================================================
 */

void setup()
{
  ESP_init();
  Display_init();
  WLAN_init();
  Bluetooth_init();

  // Ergometer an Core 1 pinnen (Echtzeitsteuerung)
  xTaskCreatePinnedToCore(ErgometerLoop, "ErgometerTask", 10000, NULL, 1, &ErgometerTask, 1);

  // Gesamte kabellose Kommunikation an Core 0 pinnen (WLAN & Bluetooth)
  xTaskCreatePinnedToCore(wirelessConnectivityLoop, "WirelessTask", 4000, NULL, 1, &WirelessTask, 0);
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// Task 1: Läuft auf Core 1 (Reiner Echtzeit-Ergometer-Kram)
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

// Task 2: Läuft im Hintergrund auf Core 0 (WLAN OTA & BLE Pulsgurt)
void wirelessConnectivityLoop(void *pvParameters)
{
  Serial.print("Wireless-Task gestartet auf Core: ");
  Serial.println(xPortGetCoreID());
  for (;;)
    {
      handleWlanOTA();                // Lauscht auf Core 0 nach OTA-Updates
      handleHeartRateConnection();    // Behandelt Pulsgurt-Verbindungen auf Core 0
      vTaskDelay(pdMS_TO_TICKS(100)); // 100ms CPU-Entlastung für Core 0
    }
}
