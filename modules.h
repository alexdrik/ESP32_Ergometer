#ifndef MODULES_H
#define MODULES_H

#include "systemdata.h"

// ==============================================================================
inline volatile uint32_t rxControlMode = 0;
inline volatile uint32_t rxTargetPower = 0;
inline volatile float rxTargetSlope = 0.0f;
inline volatile bool rxBtConnected = false;

inline volatile int32_t rxHeartRate = 0;
inline volatile int32_t rxHrBattery = 100;
inline volatile bool rxHrConnected = false;

// ==============================================================================

inline const BLEUUID hrServiceUUID("0000180d-0000-1000-8000-00805f9b34fb");
inline const BLEUUID hrCharUUID("00002a37-0000-1000-8000-00805f9b34fb");
inline const BLEUUID batServiceUUID("0000180f-0000-1000-8000-00805f9b34fb");
inline const BLEUUID batCharUUID("00002a19-0000-1000-8000-00805f9b34fb");

inline const BLEUUID ftmsServiceUUID("00001826-0000-1000-8000-00805f9b34fb");
inline const BLEUUID ftmsControlPointUUID("00002ad9-0000-1000-8000-00805f9b34fb");
inline const BLEUUID ftmsBikeDataUUID("00002ad2-0000-1000-8000-00805f9b34fb");

// ==============================================================================

// Haupt-Steuerungsschleife
void main_prg();

// 1. Initialisierung
void ESP_init();
void WLAN_init();
void handleWlanOTA();
void Bluetooth_init();
void Display_init();

// 2. Inputs
void getFTMSInputs();
void getPulsInputs();
void getGPIOInputs();

// 3. Processing
void calcCtrlMode();
void calcGear();
void calcMotorPos();

// 4. Outputs
void setGPIOOutputs();
void setFTMSOutputs();

// 5. Diagnose & Display
void showDiagData();
void showDisplayDiag();
void setField(int32_t row, int32_t col, const char *label, const char *valueText);

#endif
