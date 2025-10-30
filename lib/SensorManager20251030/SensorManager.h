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
  static void init(uint32_t ivUpdateMs = 100);

  static void update();                 // door TimerSystem aangeroepen
  static bool readEvent(SensorEvent&);  // ContextManager consumeert
  static void showStatus(bool verbose);

  static void setDistanceMillimeters(float value);
  static float distanceMillimeters();

  static void setAmbientLux(float value);
  static float ambientLux();

  static void setBoardTemperature(float value);
  static float boardTemperature();

  static void setBoardVoltage(float value);
  static float boardVoltage();

private:
  static void trampoline();
  static void enqueue(const SensorEvent& ev);
  static bool schedulePolling(uint32_t intervalMs);
  static void ensureBasePolling(uint32_t now);
  static void requestFastPolling(uint32_t now);
};
