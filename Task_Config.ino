#include "modules.h"

/*
Get-ChildItem -Recurse -Include *.cpp, *.h, *.ino | ForEach-Object {Write-Output
"==`nFILE: $($_.FullName)`n==`n"; Get-Content $_.FullName;Write-Output "`n`n" }
| Out-File -FilePath "Gesamter_Code.txt" -Encoding utf8 Get-Content
Gesamter_Code.txt | Set-Clipboard

* ==============================================================================
 *                         PROGRAMMAUFRUF-HIERARCHIE
 * ==============================================================================
 * [Core 0] - System & Connectivity (Ausgeführt von FreeRTOS im Hintergrund)
 *   +-- WLAN_init()                (Verbindung zum Netzwerk herstellen)
 *   +-- Bluetooth_init()           (BLE-Server & FTMS-Dienste starten)
 * ------------------------------------------------------------------------------
 * [Core 1] - Applikation & Hardware-Steuerung (Haupt-Task)
 *   +-- setup()                    (Einstiegspunkt für Core 1)
 *   |     +-- ESP_init()           (Serielle Schnittstelle & GPIOs
konfigurieren)
 *   |     +-- xTaskCreatePinned... (ErgometerTask erstellen und an Core 1
binden)
 *   +-- loop()                     (Leerlauf / Schläft für Watchdog-Stabilität)
 *   +-- ErgometerLoop()            (Endlosschleife auf Core 1, Intervall: 10ms)
 *         +-- main_prg()           (Das zentrale Steuerungsprogramm)
 *               |-- [INPUTS EINLESEN]
 *               |     +-- getFTMSInputs()   (Empfängt Vorgaben von
Fitness-Apps)
 *               |     +-- getPulsInputs()   (Pulsdaten von Sensor/Brustgurt
holen)
 *               |     +-- getGPIOInputs()   (Physische Taster & Reed-Sensor
lesen)
 *               |-- [VERARBEITUNG & BERECHNUNG]
 *               |     +-- calcCtrlMode()    (Regelungsmodus bestimmen:
Watt/Manuell)
 *               |     +-- calcGear()        (Virtuellen Gang berechnen)
 *               |     +-- calcMotorPos()    (Stellmotor-Sollposition berechnen)
 *               |-- [OUTPUTS SCHREIBEN]
 *               |     +-- setGPIOOutputs()  (Stellmotor & Hardware ansteuern)
 *               |     +-- setFTMSOutputs()  (Trainingsdaten via BLE an App
senden)
 *               |-- [DIAGNOSE]
 *                     +-- showDiagData()    (Konsolenausgabe alle 1000ms auf
Core 1)
 * ==============================================================================
 */

TaskHandle_t ErgometerTask;

void setup()
{
  ESP_init();
  Display_init();
  WLAN_init();
  Bluetooth_init();

  xTaskCreatePinnedToCore(ErgometerLoop, "ErgometerTask", 10000, NULL, 1,
                          &ErgometerTask, 1);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }

// Der eigentliche Programmlauf für dein Ergometer auf Core 1
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
