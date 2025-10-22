// lib/SensorManager20251004/SensorManager.h
#pragma once
#include <Arduino.h>
#include "TimerManager.h"

struct SensorEvent {
  uint8_t  type;
  uint8_t  a;
  uint16_t b;
  uint32_t value;
  uint32_t ts_ms;
};

class SensorManager {
public:
  static void init(uint8_t adcPin,
                   uint32_t ivUpdateMs,
                   uint16_t thrHigh = 2000,
                   uint16_t thrLow  = 2000,
                   uint16_t adcPollMs = 200);

  static void update();                 // door TimerSystem aangeroepen
  static bool readEvent(SensorEvent&);  // ContextManager consumeert
  static void showStatus(bool verbose);

private:
  static void trampoline();
  static void enqueue(const SensorEvent& ev);
};
