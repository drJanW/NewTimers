#include "BootMaster.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "ConductManager.h"

BootMaster bootMaster;

static TimerCallback clockCb = nullptr;
static bool clockRunning = false;
static bool clockInFallback = false;

bool BootMaster::begin() {
    waitStartMs = 0;
    fallbackSeedAttempted = false;
    fallbackSeededFromCache = false;
    fallbackStateAnnounced = false;
    return ensureTimer();
}

bool BootMaster::ensureTimer() {
    if (timerArmed) {
        return true;
    }
    if (TimerManager::instance().create(1000, 0, timerThunk)) {
        timerArmed = true;
        return true;
    }
    PL("[Conduct] BootMaster failed to arm bootstrap timer");
    return false;
}

void BootMaster::timerThunk() {
    bootMaster.bootstrapTick();
}

void BootMaster::bootstrapTick() {
    auto &clock = PRTClock::instance();
    //TODO Github Copilot  fucked up a timer here
    uint32_t now = millis();

    if (isTimeFetched()) {
        waitStartMs = 0;
        fallbackSeedAttempted = false;
        fallbackSeededFromCache = false;
        fallbackStateAnnounced = false;

        bool wasRunning = ConductManager::isClockRunning();
        bool wasFallback = ConductManager::isClockInFallback();
        if (!wasRunning || wasFallback) {
            if (ConductManager::intentStartClockTick(false)) {
                if (!wasRunning) {
                    PF("[Conduct] Clock tick started with NTP (%02u:%02u:%02u)\n",
                       clock.getHour(), clock.getMinute(), clock.getSecond());
                } else if (wasFallback) {
                    PF("[Conduct] Clock tick promoted to NTP (%02u:%02u:%02u)\n",
                       clock.getHour(), clock.getMinute(), clock.getSecond());
                }
            } else {
                PL("[Conduct] Failed to start clock tick with NTP");
            }
        }
        return;
    }

    if (waitStartMs == 0) {
        waitStartMs = now;
    }

    if (ConductManager::isClockRunning() && ConductManager::isClockInFallback()) {
        if (!fallbackStateAnnounced) {
            fallbackStateAnnounced = true;
            PL("[Conduct] Subsystems running with fallback time");
        }
        return;
    }

    if ((int32_t)(now - waitStartMs) < (int32_t)60000) {
        return;
    }

    if (!fallbackSeedAttempted) {
        fallbackSeedAttempted = true;
        // TODO: fetchLoadCachedTime is not available or not implemented in this build.
        // if (fetchLoadCachedTime(clock)) {
        //     fallbackSeededFromCache = true;
        //     PL("[Conduct] Seeded clock from cached SD snapshot");
        // } else {
        //     PL("[Conduct] No cached clock snapshot available for fallback");
        // }
    }

    bool wasFallback = ConductManager::isClockInFallback();
    if (ConductManager::intentStartClockTick(true)) {
        waitStartMs = now;
        fallbackStateAnnounced = false;
        if (!wasFallback) {
            if (fallbackSeededFromCache) {
                PL("[Conduct] Clock tick running in fallback mode (seeded)");
            } else {
                PL("[Conduct] Clock tick running in fallback mode");
            }
        }
    } else {
        PL("[Conduct] Failed to start clock tick in fallback mode");
        fallbackSeedAttempted = false;
    }
}
