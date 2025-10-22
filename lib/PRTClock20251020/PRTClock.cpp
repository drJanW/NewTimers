// =============================================
// PRTClock.cpp
// =============================================

#include <atomic>
#include <math.h>
#include "PRTClock.h"
#include "Globals.h"

PRTClock& PRTClock::instance() {
  static PRTClock inst;
  return inst;
}

// --- Atomic storage for all fields ---
static std::atomic<uint8_t>  _valHour{0}, _valMinute{0}, _valSecond{0};
static std::atomic<uint8_t>  _valYear{0}, _valMonth{1}, _valDay{1}, _valDoW{0};
static std::atomic<uint16_t> _valDoY{1};
static std::atomic<uint8_t>  _valSunriseHour{0}, _valSunriseMinute{0};
static std::atomic<uint8_t>  _valSunsetHour{0},  _valSunsetMinute{0};
static std::atomic<float>    _valMoonPhase{0.0f};

// ===================================================
// Lifecycle
// ===================================================
void PRTClock::update() {
  // advance one second
  uint8_t ss = getSecond();
  uint8_t mm = getMinute();
  uint8_t hh = getHour();

  if (++ss >= 60) {
    ss = 0;
    if (++mm >= 60) {
      mm = 0;
      if (++hh >= 24) hh = 0;
    }
  }

  setSecond(ss);
  setMinute(mm);
  setHour(hh);

  // once per midnight, trigger re-sync
  static bool synced_today = false;
  if (hh == 0 && mm == 0 && ss > 1 && !synced_today) {
    // Instead of calling fetchCurrentTime() directly,
    // just reset the flag so fetchManager's NTP retry
    // callback will handle it asynchronously.
    setTimeFetched(false);

    setDoW(getYear(), getMonth(), getDay());
    setDoY(getYear(), getMonth(), getDay());
    setMoonPhaseValue();

    synced_today = true;
  }
  if (hh == 1 && mm == 0 && ss > 0) {
    synced_today = false; // re-arm for next midnight
  }
}

// ===================================================
// Time setters/getters
// ===================================================
uint8_t PRTClock::getHour() const   { return getMux(&_valHour); }
void    PRTClock::setHour(uint8_t v){ setMux(v, &_valHour); }

uint8_t PRTClock::getMinute() const { return getMux(&_valMinute); }
void    PRTClock::setMinute(uint8_t v){ setMux(v, &_valMinute); }

uint8_t PRTClock::getSecond() const { return getMux(&_valSecond); }
void    PRTClock::setSecond(uint8_t v){ setMux(v, &_valSecond); }

void PRTClock::setTime(uint8_t hh, uint8_t mm, uint8_t ss) {
  setHour(hh); setMinute(mm); setSecond(ss);
}

// ===================================================
// Date setters/getters
// ===================================================
uint8_t PRTClock::getYear() const   { return getMux(&_valYear); }
void    PRTClock::setYear(uint8_t v){ setMux(v, &_valYear); }

uint8_t PRTClock::getMonth() const  { return getMux(&_valMonth); }
void    PRTClock::setMonth(uint8_t v){ setMux(v, &_valMonth); }

uint8_t PRTClock::getDay() const    { return getMux(&_valDay); }
void    PRTClock::setDay(uint8_t v) { setMux(v, &_valDay); }

uint8_t PRTClock::getDoW() const    { return getMux(&_valDoW); }
void    PRTClock::setDoW(uint8_t v) { setMux(v, &_valDoW); }

void PRTClock::setDoW(uint8_t y, uint8_t m, uint8_t d) {
  if (m < 3) { m += 12; y -= 1; }
  uint8_t K = y % 100, J = y / 100;
  uint16_t h = (d + 13 * (m + 1) / 5 + K + K/4 + J/4 + 5*J) % 7;
  setDoW((uint8_t)((h + 6) % 7));
}

uint16_t PRTClock::getDoY() const   { return getMux(&_valDoY); }
void     PRTClock::setDoY(uint8_t y, uint8_t m, uint8_t d) {
  uint16_t doy = d;
  for (uint8_t i = 1; i < m; ++i) {
    switch (i) {
      case 1: case 3: case 5: case 7: case 8: case 10: case 12: doy += 31; break;
      case 4: case 6: case 9: case 11: doy += 30; break;
      case 2: doy += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28; break;
    }
  }
  setMux(doy, &_valDoY);
}

// ===================================================
// Sunrise / Sunset
// ===================================================
uint8_t PRTClock::getSunriseHour() const   { return getMux(&_valSunriseHour); }
void    PRTClock::setSunriseHour(uint8_t v){ setMux(v, &_valSunriseHour); }

uint8_t PRTClock::getSunriseMinute() const { return getMux(&_valSunriseMinute); }
void    PRTClock::setSunriseMinute(uint8_t v){ setMux(v, &_valSunriseMinute); }

uint8_t PRTClock::getSunsetHour() const    { return getMux(&_valSunsetHour); }
void    PRTClock::setSunsetHour(uint8_t v) { setMux(v, &_valSunsetHour); }

uint8_t PRTClock::getSunsetMinute() const  { return getMux(&_valSunsetMinute); }
void    PRTClock::setSunsetMinute(uint8_t v){ setMux(v, &_valSunsetMinute); }

// ===================================================
// Moon phase
// ===================================================
float PRTClock::getMoonPhaseValue() const { return getMux(&_valMoonPhase); }

static float calcMoon() {
  auto &clock = PRTClock::instance();
  int y = clock.getYear(), m = clock.getMonth(), d = clock.getDay();
  if (m <= 2) { y--; m += 12; }
  const float jd0 = 2451550.1f, cycle = 29.53058867f;
  int A = y / 100, B = 2 - A + (A / 4);
  float jd = int(365.25f * (y + 4716)) + int(30.6001f * (m + 1)) + d + B - 1524.5f;
  float phase = fmodf(jd - jd0, cycle);
  if (phase < 0) phase += cycle;
  return 100.0f * 0.5f * (1.0f - cosf(2.0f * float(M_PI) * (phase / cycle)));
}

void PRTClock::setMoonPhaseValue() { setMux(calcMoon(), &_valMoonPhase); }

// ===================================================
// Debug
// ===================================================
void PRTClock::showTimeDate() const {
  PF(" Time: %02u:%02u:%02u (%u-%02u-%02u)\n",
     getHour(), getMinute(), getSecond(),
     (unsigned)(2000 + getYear()), getMonth(), getDay());
}

// ===================================================
// Global instance
// ===================================================
