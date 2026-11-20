#include "modules.h"

void main_prg()
{
  // 1. Inputs einlesen -> getInputs.cpp
  getFTMSInputs();
  getPulsInputs();
  getGPIOInputs();

  // 2. Verarbeitung/Berechnung -> processing.cpp
  calcCtrlMode();
  calcGear();
  calcMotorPos();

  // 3. Outputs schreiben -> setOutputs.cpp
  setGPIOOutputs();
  setFTMSOutputs();

  // 4. Diagnose ausgeben -> diag_Display.cpp
  showDiagData();
}
