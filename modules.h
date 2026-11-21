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

// 5. Diagnose & Display
void showDiagData();
void showDisplayDiag();
void setField(int32_t row, int32_t col, const char *label, const char *valueText);

#endif
