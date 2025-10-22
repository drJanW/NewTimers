// lib/SensorManager20251004/internal/ADC1_CH5.h   (privé, niet globaal includen)
#pragma once
#include <Arduino.h>
#include "SensorManager.h"   // voor SensorEvent type

namespace ADC1_CH5 {

struct Status {
  uint8_t  pin;
  uint16_t thrHigh, thrLow, pollMs;
  uint16_t raw;
  uint8_t  high;
  uint32_t highDurMs;
  uint8_t  fired1, fired3, fired5;
};

void begin(uint8_t adcPin, uint16_t thrHigh, uint16_t thrLow, uint16_t pollMs);
void update();                       // nonblocking poll (aanroepen door SensorManager)
bool readEvent(SensorEvent& ev);     // levert TYPE=0x20, a ∈ {1,3,5}
Status getStatus();

} // namespace ADC1_CH5
