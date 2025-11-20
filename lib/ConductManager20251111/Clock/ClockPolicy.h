#pragma once

#include <Arduino.h>

class PRTClock;

namespace ClockPolicy {

void configureHardware();
bool isRtcAvailable();
bool seedClockFromRTC(PRTClock &clock);
void syncRTCFromClock(const PRTClock &clock);
float lastTemperatureC();
bool wasPowerLost();

}
