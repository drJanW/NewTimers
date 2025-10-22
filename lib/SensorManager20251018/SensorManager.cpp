// lib/SensorManager20251004/SensorManager.cpp
#include "SensorManager.h"
#include "Globals.h"
#include "ADC1_CH5.h"

namespace {
  constexpr uint8_t Q_MASK = 0x0F;          // queue size 16
  SensorEvent q[Q_MASK + 1];
  uint8_t qHead = 0, qTail = 0;

  uint32_t s_ivUpdateMs = 0;
  uint16_t s_adcPollMs  = 0;
  uint32_t s_pollAccMs  = 0;

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

void SensorManager::init(uint8_t adcPin,
                         uint32_t ivUpdateMs,
                         uint16_t thrHigh,
                         uint16_t thrLow,
                         uint16_t adcPollMs)
{
  s_ivUpdateMs = ivUpdateMs ? ivUpdateMs : 100;
  s_adcPollMs  = adcPollMs  ? adcPollMs  : 200;
  s_pollAccMs  = 0;

  ADC1_CH5::begin(adcPin, thrHigh, thrLow, s_adcPollMs);

  auto &tm = TimerManager::instance();
  tm.cancel(SensorManager::trampoline);
  if (!tm.create(s_ivUpdateMs, 0, SensorManager::trampoline)) {
    PF("[SensorManager] Failed to schedule update timer\n");
  }
}

void SensorManager::update()
{
  s_pollAccMs += s_ivUpdateMs;
  if (s_pollAccMs >= s_adcPollMs) {
    s_pollAccMs = 0;
    ADC1_CH5::update();
  }

  SensorEvent ev;
  while (ADC1_CH5::readEvent(ev)) {
    if (ev.ts_ms == 0) ev.ts_ms = now_ms();
    enqueue(ev);
  }
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
  ADC1_CH5::Status st = ADC1_CH5::getStatus();
  if (!verbose) {
    PF("[SM] ADC1_CH5 raw=%u high=%u dur=%ums\n",
                  st.raw, st.high, (unsigned)st.highDurMs);
    return;
  }
  PF("[SM] ADC1_CH5 pin=%u thrH/L=%u/%u poll=%ums raw=%u high=%u dur=%ums f:%u%u%u\n",
                st.pin, st.thrHigh, st.thrLow, (unsigned)st.pollMs,
                st.raw, st.high, (unsigned)st.highDurMs,
                st.fired1, st.fired3, st.fired5);
}
