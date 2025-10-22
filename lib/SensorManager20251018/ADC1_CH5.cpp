// lib/SensorManager20251004/ADC1_CH5.cpp  (privé implementatie)
#include "ADC1_CH5.h"

namespace {
  uint8_t  s_pin = 0xFF;
  uint16_t s_thrHigh = 2000, s_thrLow = 2000, s_pollMs = 200;

  bool     s_high = false;
  uint32_t s_highDurMs = 0;
  bool     s_fired1 = false, s_fired3 = false, s_fired5 = false;

  uint16_t s_raw = 0;

  constexpr uint8_t Q_MASK = 0x03; // queue size 4
  SensorEvent q[Q_MASK + 1];
  uint8_t qHead = 0, qTail = 0;

  inline uint8_t inc(uint8_t i){ return uint8_t((i+1)&Q_MASK); }
  inline uint32_t now_ms(){ return (uint32_t)millis(); }
  inline uint16_t readRaw(){ return (uint16_t)analogRead(s_pin); }

  void resetHold(){
    s_highDurMs = 0;
    s_fired1 = s_fired3 = s_fired5 = false;
  }

  void pushHold(uint8_t r, uint16_t raw){
    SensorEvent ev{};
    ev.type  = 0x20;    // TYPE_ADC_HOLD
    ev.a     = r;       // 1/3/5
    ev.value = raw;
    ev.ts_ms = now_ms();
    uint8_t n = inc(qHead);
    if (n == qTail) qTail = inc(qTail);
    q[qHead] = ev;
    qHead = n;
  }
}

void ADC1_CH5::begin(uint8_t adcPin, uint16_t thrHigh, uint16_t thrLow, uint16_t pollMs)
{
  s_pin = adcPin;
  s_thrHigh = thrHigh;
  s_thrLow  = thrLow;
  s_pollMs  = pollMs ? pollMs : 200;

  pinMode(s_pin, INPUT);
  analogReadResolution(12);
  (void)analogRead(s_pin);
  s_high = false;
  resetHold();
  s_raw = 0;

  // geen eigen timer; SensorManager klokt update()
}

void ADC1_CH5::update()
{
  s_raw = readRaw();
  bool nextHigh = s_high ? (s_raw >= s_thrLow) : (s_raw > s_thrHigh);

  if (!s_high && nextHigh) { s_high = true;  resetHold(); }
  else if (s_high && !nextHigh){ s_high = false; resetHold(); }

  if (s_high) {
    s_highDurMs += s_pollMs;
    if (!s_fired1 && s_highDurMs >= 1000){ pushHold(1, s_raw); s_fired1 = true; }
    if (!s_fired3 && s_highDurMs >= 3000){ pushHold(3, s_raw); s_fired3 = true; }
    if (!s_fired5 && s_highDurMs >= 5000){ pushHold(5, s_raw); s_fired5 = true; }
  }
}

bool ADC1_CH5::readEvent(SensorEvent& out)
{
  if (qHead == qTail) return false;
  out = q[qTail];
  qTail = inc(qTail);
  return true;
}

ADC1_CH5::Status ADC1_CH5::getStatus()
{
  Status s{};
  s.pin = s_pin;
  s.thrHigh = s_thrHigh; s.thrLow = s_thrLow; s.pollMs = s_pollMs;
  s.raw = s_raw;
  s.high = s_high ? 1 : 0;
  s.highDurMs = s_highDurMs;
  s.fired1 = s_fired1; s.fired3 = s_fired3; s.fired5 = s_fired5;
  return s;
}
