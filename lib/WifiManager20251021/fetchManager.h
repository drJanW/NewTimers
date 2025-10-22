// =============================================
// fetchManager.h – TimerManager-driven version
// =============================================

#pragma once
#include <Arduino.h>

class PRTClock;

bool bootFetchManager();
void updateFetchManager();      // legacy poll hook (currently unused)

bool fetchLoadCachedTime(PRTClock &clock);

