#ifndef CONFIG_H
#define CONFIG_H

/*
lila Ader           -> +11.5V Spannungsversorgung
dunkelblaue Ader    -> Motor negativ (GPIO 2 über DRV8833 Out1)
gruene Ader          -> Motor positiv (GPIO 1 über DRV8833 Out2)
gelbe Ader          -> Inkrementalgeber (GPIO 11)
orange Ader         -> ???
rote Ader           -> Referenzzschalter INkrementalgeber
braune Ader         -> Ground Spannungsversorgung
schwarze Ader       -> Reed-Schalter für Kadenzmessung
*/

// GPIO Pins left
#define MOTOR_POS_PIN 1    // (ADC1 CH0) Output-Pin fuer Motorrichtung positiv
#define MOTOR_NEG_PIN 2    // (ADC1 CH1) Output-Pin fuer Motorrichtung negativ
#define RESERVE_PIN_3 3    // (ADC1 CH2)
#define KADENCE_PIN 10     // (SPI2 CS)  Input-Pin fuer Kadenz-Messung
#define INKREMENTAL_PIN 11 // (SPI2 D)   Input-Pin fuer Inkrementalgeber-Messung
#define REFERENCE_PIN 12   // (SPI2 CLK) Input-Pin fue Inkrementalgeber-Referenz
#define RESERVE_PIN_13 13  // (SPI2 Q)

// GPIO Pins right
#define PIN_UART_TX 43    // (UART2 Tx)
#define PIN_UART_RX 44    // (UART2 Rx)
#define RESERVE_PIN_18 18 // (UART1 Rx)
#define RESERVE_PIN_17 17 // (UART1 Tx)
#define GEAR_MINUS 21     //            Input-Pin fuer leichteren Gang
#define GEAR_PLUS 16      //            Input-Pin fuer schwereren Gang

// GPIO Pins Onboard Buttons
#define MENU_SWITCH 0 // (Button)   Input-Pin fuer Menü-Umschaltung
#define MENU_EDIT 14  // (Button)   Input-Pin fuer Menü-Bedienung

// T-Display-S3 Onboard-Display Konfiguration
#define PIN_POWER_ON 15 // (Display)  Schaltet die Stromversorgung des Displays ein
#define PIN_LCD_BL 38   // Hintergrundbeleuchtung (Backlight)

// System-Konstanten
#define DEBOUNCE_DELAY 150 // Entprellzeit in ms
#define DIAG_INTERVAL 1000 // Diagnose-Intervall in ms

// Physikalische Konstanten für die Fahrrad-Simulation
const float GRAVITY_G = 9.81f;       // Erdbeschleunigung in m/s^2
const float CR_ROLL_RESIST = 0.004f; // Rollwiderstandsbeiwert (guter Rennradreifen)
const float CW_AERO = 0.32f;         // Luftwiderstandsbeiwert * Stirnfläche (Aero-Faktor)

// Gesamtgewicht von Fahrer und Fahrrad (in kg für die Leistungsberechnung am Berg)
const float TOTAL_SYSTEM_WEIGHT_KG = 120.0f; // z.B. 105kg Fahrer + 15kg Ergometer
const int32_t CARDIO_TARGET_HR = 130;        // Soll-Puls: 130 bpm

// Radumfang-Konfiguration (in Millimetern für die präzise Berechnung)
const float WHEEL_CIRCUMFERENCE_MM = 2200.0f;

// Gangschaltung Übersetzungen (z.B. 3 x 9 Gänge)
const uint32_t GEAR_COUNT = 13;
const float GEAR_RATIOS[GEAR_COUNT] = {
    0.99f, // Gang 1 - 1:
    1.33f, // Gang 1 - 2:
    1.68f, // Gang 1 - 3:
    2.06f, // Gang 1 - 4:
    2.44f, // Gang 2 - 3:
    3.00f, // Gang 2 - 4:
    3.47f, // Gang 2 - 5:
    3.88f, // Gang 2 - 6:
    4.39f, // Gang 2 - 7:
    5.09f, // Gang 3 - 5;
    5.77f, // Gang 3 - 6:
    6.66f, // Gang 3 - 7:
    7.21f  // Gang 3 - 8:
};

// Motorpositionen (z.B. 24 Stufen)
const uint32_t MOTOR_POS_COUNT = 24;
const uint32_t MOTOR_NEG_RELPOS = 32;
const uint32_t MOTOR_ABSPOS[MOTOR_POS_COUNT] = {
    //  inkr   Gang  680-inkr  diff  Formel      diff
    0,   //  1 - 680                  0
    140, //  2 - 540      -140      147       -147
    234, //  3 - 446       -94      233        -86
    284, //  4 - 396       -50      294        -61
    344, //  5 - 336       -60      341        -47
    374, //  6 - 306       -30      379        -38
    410, //  7 - 270       -36      412        -33
    440, //  8 - 240       -30      440        -28
    468, //  9 - 212       -28      465        -25
    488, // 10 - 192       -20      488        -23
    510, // 11 - 170       -22      508        -20
    528, // 12 - 152       -18      526        -18
    546, // 13 - 134       -18      543        -17
    558, // 14 - 122       -12      559        -16
    572, // 15 - 108       -14      574        -15
    586, // 16 -  94       -14      587        -13
    604, // 17 -  76       -18      600        -13
    614, // 18 -  66       -10      612        -12
    624, // 19 -  56       -10      624        -12
    634, // 20 -  46       -10      634        -10
    644, // 21 -  36       -10      645        -11
    654, // 22 -  26       -10      655        -10
    664, // 23 -  16       -10      664         -9
    680  // 24 -   0       -16      673         -9

    // Inkremente = 211.8 * logf(Gang);
    // Drehmoment = 0.644 * Gang +2.22;
    // Ergometer-Leistung = Gang * (Kadenz^1.7) / 82

    // Kinomap sendet Steigung s, Rollwiderstandswert cr, Luftwiderstandsbeiwert cw und Windgeschwindigkeit vw
    // Systemgewicht m ist bekannt, Istgeschwindigkeit v_act wird vom vorherigen Schritt übernommen
    // Hangabtriebstkraft Fg = m * g * sin(atan(s));
    // Rollwiderstandskraft Fr = m * g * cr * cos(atan(s));
    // Luftwiderstandskraft Fa = cw * (vw + v_act)^2
    // Gesamtwiderstandskraft Fges = Fg + Fr + Fa;
    // aus dem Gesamtwiderstand, der Getriebeübersetzung und dem Radumfang wird das erforderliche Drehmoment an der Kurbel berechnet.
    // Kurbelmoment Mk = Fges * (Radumfang / (2 * pi)) / Gangübersetzung;
    // aus dem Kurbelmoment wird die Inkremente-Anzahl des Stellmotors berechnet.
    // Inkremente = 211.8 * logf(1.553 * (Mk - 2.22) );
    // vom Fahrer erbrachte Kadenz auf Fahrradgeschwindigkeit umrechnen
    // Kadenz * Gangübersetzung = Fahrergeschwindigkeit v_h
    // wenn v_h > v_act, beschleunigt der Fahrer das Fahrrad mit seiner Ergometer-Leistung
    // Ergometer-Leistung Pe = e^(Inkremente/211.8) * (Kadenz^1.7) / 82
    // wenn v_h < v_act, rollt das Fahrrad aus
    // Pe = 0;
    // a = (Pe/v_act - Fges) / m;
    // v_act_neu = v_act + a * dt;

    // PWM-Einstellungen für das DRV8833 (20kHz Frequenz, 8-Bit Auflösung -> 0 bis 255)
    const uint32_t PWM_FREQ = 20000;
const uint8_t PWM_RES_BITS = 8;
const uint8_t CH_MOTOR_POS = 0; // PWM Kanal 0
const uint8_t CH_MOTOR_NEG = 1; // PWM Kanal 1

const uint32_t REGULATOR_INTERVAL_MS = 10000; // Regel-Intervall: 10 Sekunden (10000 ms)
#endif
