#include "config.h"
#include "modules.h"
#include "system_data.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

const uint32_t COL_WIDTH_PX = 64;
const uint32_t ROW_HEIGHT_PX = 8;

// Super einfache, universelle Hilfsfunktion zum Beschreiben der Display-Matrix!
void setField(int32_t row, int32_t col, const char *label, const char *valueText)
{
  if (row < MATRIX_ROWS && col < MATRIX_COLS)
    {
      // Baut das Feld direkt im Format "LABEL:WERT" (z.B. "RPM :  85") zusammen
      snprintf(systemData.displayMatrix[row][col],
               COL_CHAR_LIMIT + 1,
               "%-4s:%5s",
               label,
               valueText);
    }
}

void showDisplayDiag()
{
  static char lastMatrix[MATRIX_ROWS][MATRIX_COLS][COL_CHAR_LIMIT + 1] = {0};

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  for (int32_t r = 0; r < MATRIX_ROWS; r++)
    {
      for (int32_t c = 0; c < MATRIX_COLS; c++)
        {
          if (strcmp(systemData.displayMatrix[r][c], lastMatrix[r][c]) != 0)
            {
              uint32_t x = c * COL_WIDTH_PX;
              uint32_t y = r * ROW_HEIGHT_PX;

              tft.drawString(systemData.displayMatrix[r][c], x, y);

              strncpy(lastMatrix[r][c], systemData.displayMatrix[r][c], COL_CHAR_LIMIT);
              lastMatrix[r][c][COL_CHAR_LIMIT] = '\0';
            }
        }
    }
}

void showDiagData()
{
  static uint32_t lastDiagTime = 0;
  if (millis() - lastDiagTime > DIAG_INTERVAL)
    {
      lastDiagTime = millis();

      // Puffer für die String-Umwandlung der Werte
      char valBuf[6];

      // --- REIHE 0 ---
      itoa(systemData.controlMode, valBuf, 10);
      setField(0, 0, "MODE", valBuf);

      itoa(systemData.cadenceRPM, valBuf, 10);
      setField(0, 1, "RPM", valBuf);

      itoa(systemData.currentPowerWatt, valBuf, 10);
      setField(0, 2, "WATT", valBuf);

      itoa(systemData.targetMotorPosition, valBuf, 10);
      setField(0, 3, "MOT", valBuf);

      itoa(millis() / 1000, valBuf, 10);
      setField(0, 4, "UP", valBuf);

      // --- REIHE 1 ---
      itoa(systemData.currentGear, valBuf, 10);
      setField(1, 0, "GEAR", valBuf);

      itoa(systemData.heartRateBPM, valBuf, 10);
      setField(1, 1, "PULS", valBuf);

      // Kontinuierlicher Float-Wert mit genau einer Nachkommastelle formatieren! (z.B. "28.4")
      dtostrf(systemData.speedKmH, 4, 1, valBuf);
      setField(1, 2, "KMH", valBuf);

      // Kontinuierlicher Float-Wert für die Steigung (z.B. "-1.5" oder "+5.5")
      // Das '+' Zeichen erzwingt ein sichtbares Plus oder Minus
      snprintf(valBuf, sizeof(valBuf), "%+4.1f", systemData.targetSlope);
      setField(1, 3, "SLOP", valBuf);

      // Trigger das flimmerfreie Zeichnen
      showDisplayDiag();
    }
}
