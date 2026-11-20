#ifndef MODULES_H
#define MODULES_H

// Haupt-Steuerungsschleife
void main_prg();

// 1. Initialisierung
void ESP_init();
void WLAN_init();
void WLAN_update();
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

// 5. Diagnose
void showDiagData();
void showDisplayDiag();
void setField(uint32_t row, uint32_t col, const char *label, uint32_t value);

#endif
