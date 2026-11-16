#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>

struct ErgometerData
{
    uint32_t cadenceRPM;
    uint32_t heartRateBPM;
    bool pedalPulseDetected;
    
    uint32_t currentGear;
    uint32_t targetMotorPosition;
    uint32_t controlMode; 
    
    uint32_t speedKmH;       
    uint32_t currentPowerWatt;
};

// Deklaration: Macht die Struktur für alle .cpp Dateien sichtbar
extern ErgometerData systemData;

#endif
