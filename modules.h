#ifndef MODULES_H
#define MODULES_H

// Haupt-Steuerungsschleife
void main_prg();

// 1. Initialisierung
void ESP_init();
void WLAN_init();
void Bluetooth_init();

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

// 5. Diagnose
void showDiagData();

#endif
