// lib/SensorManager20251004/SensorManager.cpp
#include "SensorManager.h"
#include "Globals.h"
#include <atomic>
#include <math.h>
#if IF_VL53L1X_PRESENT
#include "VL53L1X.h"
#ifndef VL53L1X_DEBUG
#define VL53L1X_DEBUG 0
#endif

#if VL53L1X_DEBUG
#define SENSOR_VL53_LOG(...) PF(__VA_ARGS__)
#else
#define SENSOR_VL53_LOG(...) do {} while (0)
#endif
#endif

namespace {
  constexpr uint8_t Q_MASK = 0x0F;          // queue size 16
  SensorEvent q[Q_MASK + 1];
  uint8_t qHead = 0, qTail = 0;

  uint32_t s_ivUpdateMs = 0;
  constexpr uint32_t SENSOR_BASE_DEFAULT_MS = 100;
  constexpr uint32_t SENSOR_FAST_INTERVAL_MS = 30;
  constexpr uint32_t SENSOR_FAST_DURATION_MS = 800;
  constexpr float SENSOR_FAST_DELTA_MM = 80.0f;

  uint32_t s_baseIntervalMs = SENSOR_BASE_DEFAULT_MS;
  uint32_t s_fastUntilMs = 0;
  bool s_fastActive = false;
  bool s_hasDistance = false;
  float s_lastDistanceMm = 0.0f;

  std::atomic<bool> s_distanceSampleValid{false};
  std::atomic<float> s_distanceSampleMm{0.0f};
  std::atomic<float> s_ambientLux{0.0f};
  std::atomic<float> s_boardTemperature{0.0f};
  std::atomic<float> s_boardVoltage{0.0f};

#if IF_VL53L1X_PRESENT
  bool     s_vlReady    = false;
  uint32_t s_vlLastMs   = 0;
#endif

  inline uint8_t inc(uint8_t i){ return uint8_t((i+1)&Q_MASK); }
  inline uint32_t now_ms(){ return (uint32_t)millis(); }

}

void SensorManager::enqueue(const SensorEvent& ev){
  uint8_t n = inc(qHead);
  if (n == qTail) qTail = inc(qTail);
  q[qHead] = ev;
  qHead = n;
}

void SensorManager::trampoline(){
  SensorManager::update();
}

bool SensorManager::schedulePolling(uint32_t intervalMs)
{
  if (intervalMs == 0) intervalMs = 1;
  if (s_ivUpdateMs == intervalMs && s_ivUpdateMs != 0) {
    return true;
  }

  auto &tm = TimerManager::instance();
  uint32_t previous = s_ivUpdateMs;

  if (!tm.restart(intervalMs, 0, SensorManager::trampoline)) {
    PF("[SensorManager] Failed to schedule poll interval %ums\n", (unsigned)intervalMs);
    if (previous != 0 && previous != intervalMs) {
      if (tm.restart(previous, 0, SensorManager::trampoline)) {
        s_ivUpdateMs = previous;
      }
    }
    return false;
  }

  s_ivUpdateMs = intervalMs;
  return true;
}

void SensorManager::ensureBasePolling(uint32_t now)
{
  if (!s_fastActive) {
    return;
  }
  if (now >= s_fastUntilMs && SensorManager::schedulePolling(s_baseIntervalMs)) {
    s_fastActive = false;
    s_fastUntilMs = 0;
  }
}

void SensorManager::requestFastPolling(uint32_t now)
{
  uint32_t target = now + SENSOR_FAST_DURATION_MS;
  if (s_fastActive) {
    if (target > s_fastUntilMs) {
      s_fastUntilMs = target;
    }
    return;
  }
  if (SensorManager::schedulePolling(SENSOR_FAST_INTERVAL_MS)) {
    s_fastActive = true;
    s_fastUntilMs = target;
  }
}

void SensorManager::setDistanceMillimeters(float value) {
  s_distanceSampleMm.store(value, std::memory_order_relaxed);
  s_distanceSampleValid.store(true, std::memory_order_relaxed);
}

float SensorManager::distanceMillimeters() {
  return s_distanceSampleMm.load(std::memory_order_relaxed);
}

void SensorManager::setAmbientLux(float value) {
  s_ambientLux.store(value, std::memory_order_relaxed);
}

float SensorManager::ambientLux() {
  return s_ambientLux.load(std::memory_order_relaxed);
}

void SensorManager::setBoardTemperature(float value) {
  s_boardTemperature.store(value, std::memory_order_relaxed);
}

float SensorManager::boardTemperature() {
  return s_boardTemperature.load(std::memory_order_relaxed);
}

void SensorManager::setBoardVoltage(float value) {
  s_boardVoltage.store(value, std::memory_order_relaxed);
}

float SensorManager::boardVoltage() {
  return s_boardVoltage.load(std::memory_order_relaxed);
}

void SensorManager::init(uint32_t ivUpdateMs)
{
  s_baseIntervalMs = ivUpdateMs ? ivUpdateMs : SENSOR_BASE_DEFAULT_MS;
  s_fastActive = false;
  s_fastUntilMs = 0;
  s_hasDistance = false;
  s_lastDistanceMm = 0.0f;

  SensorManager::schedulePolling(s_baseIntervalMs);

#if IF_VL53L1X_PRESENT
  if (!s_vlReady) {
    s_vlReady = VL53L1X_begin();
    if (!s_vlReady) {
      PF("[SensorManager] VL53L1X begin failed\n");
    } else {
      PF("[SensorManager] VL53L1X ready\n");
    }
  }
#endif
  if (s_ivUpdateMs == 0) {
    PF("[SensorManager] Polling interval not scheduled\n");
  }
}

void SensorManager::update()
{
  uint32_t now = now_ms();
  SensorManager::ensureBasePolling(now);

#if IF_VL53L1X_PRESENT
  if (!s_vlReady) {
    s_vlReady = VL53L1X_begin();
    if (!s_vlReady) {
      return;
    }
    PF("[SensorManager] VL53L1X reinit successful\n");
  }

  float distanceMm = readVL53L1X();
  if (!isnan(distanceMm)) {
    float deltaMm = 0.0f;
    if (s_hasDistance) {
      deltaMm = fabsf(distanceMm - s_lastDistanceMm);
    } else {
      s_hasDistance = true;
    }

    s_lastDistanceMm = distanceMm;

    if (s_hasDistance && deltaMm >= SENSOR_FAST_DELTA_MM) {
  SensorManager::requestFastPolling(now);
    }

  SensorManager::setDistanceMillimeters(distanceMm);
    s_vlLastMs = now_ms();

    SensorEvent dist{};
    dist.type  = 0x30; // TYPE_DISTANCE_MM
    dist.value = static_cast<uint32_t>(distanceMm);
    dist.ts_ms = s_vlLastMs;
    enqueue(dist);
    SENSOR_VL53_LOG("[SensorManager] VL53L1X distance=%.0fmm\n", distanceMm);
  }
#endif
}

bool SensorManager::readEvent(SensorEvent& out)
{
  if (qHead == qTail) return false;
  out = q[qTail];
  qTail = inc(qTail);
  return true;
}

void SensorManager::showStatus(bool verbose)
{
#if IF_VL53L1X_PRESENT
  float dist = SensorManager::distanceMillimeters();
  uint32_t age = s_vlLastMs ? (now_ms() - s_vlLastMs) : 0;
  if (!verbose) {
    PF("[SM] VL53L1X ready=%u distance=%.1fmm age=%ums\n",
       s_vlReady ? 1 : 0,
       dist,
       (unsigned)age);
    return;
  }
  PF("[SM] VL53L1X ready=%u last=%.1fmm age=%ums\n",
     s_vlReady ? 1 : 0,
     dist,
     (unsigned)age);
#else
  (void)verbose;
  PF("[SM] VL53L1X disabled\n");
#endif
}
