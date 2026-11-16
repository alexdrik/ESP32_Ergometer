#include "modules.h"

// Task-Handle für die Ergometer-Steuerung auf Core 1
TaskHandle_t ErgometerTask;

// Der eigentliche Programmlauf für dein Ergometer auf Core 1
void ErgometerLoop(void * pvParameters)
    {
    Serial.print("Ergometer-Task gestartet auf Core: ");
    Serial.println(xPortGetCoreID());

    for(;;) // Endlosschleife für Core 1
        {
        // 1. INPUT
        getFTMSInputs();
        getPulsInputs();
        getGPIOInputs();
        
        // 2. PROCESSING
        calcCtrlMode();
        calcGear();
        calcMotorPos();
        
        // 3. OUTPUT
        setGPIOOutputs();
        setFTMSOutputs();
        
        // 4. DIAGNOSE
        showDiagData();
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms Pause (schont den Watchdog-Timer)
        }
    }

void setup()
    {
    // Hardware initialisieren (läuft kurz auf Core 1 an)
    ESP_init();
    
    // WLAN und Bluetooth initialisieren (FreeRTOS schiebt deren Hintergrund-Prozesse auf Core 0)
    WLAN_init();
    Bluetooth_init();

    // Wir erstellen den Ergometer-Task und binden ihn fest an Core 1 (letzter Parameter = 1)
    xTaskCreatePinnedToCore(
        ErgometerLoop,     /* Funktion, die den Task ausführt */
        "ErgometerTask",   /* Name des Tasks */
        10000,             /* Stack-Größe in Words */
        NULL,              /* Task-Parameter */
        1,                 /* Priorität des Tasks */
        &ErgometerTask,    /* Task-Handle */
        1                  /* Core-ID (1) */
    );
    }

void loop()
    {
    // Der Standard-Loop bleibt leer, da unser ErgometerLoop als Task auf Core 1 läuft.
    // Core 0 kümmert sich im Hintergrund autark um WLAN/BLE.
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
