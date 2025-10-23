#pragma once
#include <Arduino.h>
#include "AudioManager.h"
#include "LightManager.h"
#include "SDManager.h"
#include "OTAManager.h"
#include "TimerManager.h"


// High-level orchestrator of system behavior
class ConductManager {
public:
    // Lifecycle
    static void begin();
    static void update();

    // Intents (external requests)
    static void intentPlayFragment();
    static void intentSayTime();
    static void intentSayNow();
    static void intentSetBrightness(float value);
    static void intentSetAudioLevel(float value);
    static void intentArmOTA(uint32_t window_s);
    static void intentConfirmOTA();
    static void intentShowTimerStatus();
    static void intentPulseStatusLed();
    static void intentSetHeartbeatRate(uint32_t intervalMs);
    static bool intentStartClockTick(bool fallbackMode);
    static bool isClockRunning();
    static bool isClockInFallback();

    // Context profiles
    static void setChristmasMode(bool enabled);
    static void setQuietHours(bool enabled);

private:
    // Internal helpers
    static void applyContextOverrides();
    static bool christmasMode;
    static bool quietHours;
};