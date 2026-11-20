#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft = TFT_eSPI();

const uint32_t COL_WIDTH_PX = 64;
const uint32_t ROW_HEIGHT_PX = 8;

void showDiagData()
{
  static uint32_t lastDiagTime = 0;
  if (millis() - lastDiagTime > DIAG_INTERVAL)
    {
      lastDiagTime = millis();

      // Einzeilige Definition: setField(REIHE, SPALTE, "LABEL", WERT)
      // Reihe 0
      setField(0, 0, "MODE", systemData.controlMode);
      setField(0, 1, "RPM", systemData.cadenceRPM);
      setField(0, 2, "WATT", systemData.currentPowerWatt);
      setField(0, 3, "MOT", systemData.targetMotorPosition);
      setField(0, 4, "UP", millis() / 1000);

      // Reihe 1
      setField(1, 0, "GEAR", systemData.currentGear);
      setField(1, 1, "PULS", systemData.heartRateBPM);
      setField(1, 2, "KMH", systemData.speedKmH);
      setField(1, 3, "PED", systemData.pedalPulseDetected ? 1 : 0);

      // Reihe 2

      // Reihe 3

      // Reihe 4

      // Grafikengine triggern: Prüft auf Änderungen und zeichnet flimmerfrei
      showDisplayDiag();
    }
}

// Super kompakte Hilfsfunktion für deine Update-Zeilen!
void setField(uint32_t row, uint32_t col, const char *label, uint32_t value)
{
  if (row < MATRIX_ROWS && col < MATRIX_COLS)
    {
      // Kopiere das Label (maximal 5 Zeichen)
      strncpy(systemData.displayLabels[row][col], label, 5);
      systemData.displayLabels[row][col][5] = '\0';

      // Weise den Wert zu
      systemData.displayValues[row][col] = value;
    }
}

// Baut aus den Labels und Werten den finalen String zusammen
void updateMatrixText()
{
  for (uint32_t r = 0; r < MATRIX_ROWS; r++)
    {
      for (uint32_t c = 0; c < MATRIX_COLS; c++)
        {
          if (strlen(systemData.displayLabels[r][c]) > 0)
            {
              snprintf(systemData.displayMatrix[r][c], COL_CHAR_LIMIT + 1,
                       "%-4s:%4d", systemData.displayLabels[r][c],
                       systemData.displayValues[r][c]);
            }
          else
            {
              strcpy(systemData.displayMatrix[r][c], "          ");
            }
        }
    }
}

void showDisplayDiag()
{
  updateMatrixText();

  static char lastMatrix[MATRIX_ROWS][MATRIX_COLS][COL_CHAR_LIMIT + 1] = {0};

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  for (uint32_t r = 0; r < MATRIX_ROWS; r++)
    {
      for (uint32_t c = 0; c < MATRIX_COLS; c++)
        {
          if (strcmp(systemData.displayMatrix[r][c], lastMatrix[r][c]) != 0)
            {
              uint32_t x = c * COL_WIDTH_PX;
              uint32_t y = r * ROW_HEIGHT_PX;

              tft.drawString(systemData.displayMatrix[r][c], x, y);

              strncpy(lastMatrix[r][c], systemData.displayMatrix[r][c],
                      COL_CHAR_LIMIT);
              lastMatrix[r][c][COL_CHAR_LIMIT] = '\0';
            }
        }
    }
}
