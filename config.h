#ifndef CONFIG_H
#define CONFIG_H

// GPIO Pins
#define MOTOR_POS_PIN 1   // Output-Pin für Motorrichtung positiv
#define MOTOR_NEG_PIN 2   // Output-Pin für Motorrichtung negativ
#define KADENCE_PIN 3     // Input-Pin für Kadenz-Messung
#define INKREMENTAL_PIN 4 // Input-Pin für Inkrementalgeber-Messung
#define REFERENCE_PIN 5   // Input-Pin für Inkrementalgeber-Referenz
#define RESERVE_PIN 6     // noch nicht benutzt

#define GEAR_PLUS 16  // Input-Pin für schwereren Gang
#define GEAR_MINUS 17 // Input-Pin für leichteren Gang

#define MENU_EDIT 4   // Input-Pin für Menü-Bedienung
#define MENU_SWITCH 0 // Input-Pin für Menü-Umschaltung

// T-Display-S3 Onboard-Display Konfiguration
#define PIN_POWER_ON 15 // Schaltet die Stromversorgung des Displays ein
#define PIN_LCD_BL 38   // Hintergrundbeleuchtung (Backlight)

// System-Konstanten
#define DEBOUNCE_DELAY 150 // Entprellzeit in ms
#define DIAG_INTERVAL 1000 // Diagnose-Intervall in ms

// Gangschaltung Übersetzungen (z.B. 15 Gänge)
// Faktor * 100 (z.B. Gang 1 = 2.57, Gang 15 = 8.44)
const uint32_t GEAR_COUNT = 13;
const float32 GEAR_RATIOS[GEAR_COUNT] = {
    0.99f, // Gang 1 - 1:
    1.33f, // Gang 1 - 2:
    1.68f, // Gang 1 - 3:
    2.06f, // Gang 1 - 4:
    2.44f, // Gang 2 - 3:
    3.00f, // Gang 2 - 4:
    3.47f, // Gang 2 - 5:
    3.88f, // Gang 2 - 6:
    4.39f, // Gang 2 - 7:
    5.09f, // Gang 3 - 6;
    5.77f, // Gang 3 - 6:
    6.66f, // Gang 3 - 7:
    7.21f  // Gang 3 - 8:
};

// Motorpositionen (z.B. 24 Stufen)
const uint32_t MOTOR_POS_COUNT = 24;
const uint32_t MOTOR_NEG_RELPOS = 32;
const uint32_t MOTOR_ABSPOS[MOTOR_POS_COUNT] = {
    0,   //  1 - 680
    140, //  2 - 540
    234, //  3 - 446
    284, //  4 - 396
    344, //  5 - 336
    374, //  6 - 306
    410, //  7 - 270
    440, //  8 - 240
    468, //  9 - 212
    488, // 10 - 192
    510, // 11 - 170
    528, // 12 - 152
    546, // 13 - 134
    558, // 14 - 122
    572, // 15 - 108
    586, // 16 -  94
    604, // 17 -  76
    614, // 18 -  66
    624, // 19 -  56
    634, // 20 -  46
    644, // 21 -  36
    654, // 22 -  26
    664, // 23 -  16
    680  // 24 -   0
};

#endif
