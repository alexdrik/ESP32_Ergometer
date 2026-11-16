#include "config.h"
#include "modules.h"
#include "system_data.h"

void showDiagData()
{
#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <TFT_eSPI.h> // Grafikbibliothek für das T-Display-S3

  TFT_eSPI tft = TFT_eSPI();

  void showDisplayDiag()
  {
    // Statischer Buffer für Formatierungen (maximal 10 Zeichen +
    // Nullterminator)
    char buf[11];

    tft.setTextSize(1);                     // Kleinste Schriftgröße aktivieren
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // Grüne Schrift auf schwarzem Grund

    /**************************************************************************/
    // Spalte 1 (x = 0): System-Modus & Status
    tft.drawString("MODE:     ", 0, 0);
    snprintf(buf, sizeof(buf), "%-10d", systemData.controlMode);
    tft.drawString(buf, 0, 8);

    tft.drawString("GEAR:     ", 0, 24);
    snprintf(buf, sizeof(buf), "%-10d", systemData.currentGear);
    tft.drawString(buf, 0, 32);

    /**************************************************************************/
    // Spalte 2 (x = 64): Sensordaten
    tft.drawString("CADENCE:  ", 64, 0);
    snprintf(buf, sizeof(buf), "%-10d", systemData.cadenceRPM);
    tft.drawString(buf, 64, 8);

    tft.drawString("PULSE:    ", 64, 24);
    snprintf(buf, sizeof(buf), "%-10d", systemData.heartRateBPM);
    tft.drawString(buf, 64, 32);

    /**************************************************************************/
    // Spalte 3 (x = 128): Leistung & Physik
    tft.drawString("POWER:    ", 128, 0);
    snprintf(buf, sizeof(buf), "%-10d", systemData.currentPowerWatt);
    tft.drawString(buf, 128, 8);

    tft.drawString("SPEED:    ", 128, 24);
    snprintf(buf, sizeof(buf), "%-10d", systemData.speedKmH);
    tft.drawString(buf, 128, 32);

    /**************************************************************************/
    // Spalte 4 (x = 192): Hardware-Feedback
    tft.drawString("MOTOR_POS:", 192, 0);
    snprintf(buf, sizeof(buf), "%-10d", systemData.targetMotorPosition);
    tft.drawString(buf, 192, 8);

    tft.drawString("PEDAL_FLG:", 192, 24);
    snprintf(buf, sizeof(buf), "%-10s",
             systemData.pedalPulseDetected ? "ACTIVE" : "WAIT");
    tft.drawString(buf, 192, 32);

    /**************************************************************************/
    // Spalte 5 (x = 256): Zeitstempel / Uptime (Uptime in Sekunden)
    tft.drawString("UPTIME:   ", 256, 0);
    snprintf(buf, sizeof(buf), "%-10d", millis() / 1000);
    tft.drawString(buf, 256, 8);
  }
}