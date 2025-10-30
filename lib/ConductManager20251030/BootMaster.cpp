#include "BootMaster.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PRTClock.h"
#include "ConductManager.h"
#include "fetchManager.h"

BootMaster bootMaster;

namespace {

constexpr uint32_t CLOCK_BOOTSTRAP_INTERVAL_MS = 1000;
constexpr uint32_t NTP_FALLBACK_TIMEOUT_MS = 60 * 1000;

TimerManager &timers() {
    return TimerManager::instance();
}

} // namespace

bool BootMaster::begin() {
    cancelFallbackTimer();
    fallback.resetFlags();
    return ensureTimer();
}

bool BootMaster::ensureTimer() {
    if (timerArmed) {
        return true;
    }
    if (timers().create(CLOCK_BOOTSTRAP_INTERVAL_MS, 0, timerThunk)) {
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

    if (isTimeFetched()) {
        cancelFallbackTimer();
        fallback.resetFlags();

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

    bool isRunning = ConductManager::isClockRunning();
    bool inFallback = ConductManager::isClockInFallback();

    if (isRunning && inFallback) {
        if (!fallback.stateAnnounced) {
            fallback.stateAnnounced = true;
            PL("[Conduct] Subsystems running with fallback time");
        }
        cancelFallbackTimer();
        return;
    }

    ensureFallbackTimer();
}

void BootMaster::fallbackThunk() {
    bootMaster.fallbackTimeout();
}

bool BootMaster::ensureFallbackTimer() {
    if (fallback.timerArmed) {
        return true;
    }
    if (timers().create(NTP_FALLBACK_TIMEOUT_MS, 1, fallbackThunk)) {
        fallback.timerArmed = true;
        return true;
    }
    PL("[Conduct] BootMaster failed to arm fallback timeout");
    return false;
}

void BootMaster::cancelFallbackTimer() {
    timers().cancel(fallbackThunk);
    fallback.timerArmed = false;
}

void BootMaster::fallbackTimeout() {
    fallback.timerArmed = false;

    if (isTimeFetched()) {
        fallback.resetFlags();
        return;
    }

    auto &clock = PRTClock::instance();

    if (!fallback.seedAttempted) {
        fallback.seedAttempted = true;
        if (fetchLoadCachedTime(clock)) {
            fallback.seededFromCache = true;
            PL("[Conduct] Seeded clock from cached SD snapshot");
        } else {
            fallback.seededFromCache = false;
            PL("[Conduct] No cached clock snapshot available for fallback");
        }
    }

    bool wasFallback = ConductManager::isClockInFallback();
    if (ConductManager::intentStartClockTick(true)) {
        fallback.stateAnnounced = false;
        if (!wasFallback) {
            if (fallback.seededFromCache) {
                PL("[Conduct] Clock tick running in fallback mode (seeded)");
            } else {
                PL("[Conduct] Clock tick running in fallback mode");
            }
        }
    } else {
        PL("[Conduct] Failed to start clock tick in fallback mode");
        fallback.seedAttempted = false;
        fallback.seededFromCache = false;
        ensureFallbackTimer();
    }
}
