#include "StatusBoot.h"
#include "Globals.h"
#include "StatusPolicy.h"
#include "PRTClock.h"
#include "ContextManager.h"

StatusBoot statusBoot;

void timeDisplayTick() {
    auto &clock = PRTClock::instance();
    bool clockSeeded = isTimeFetched() || clock.getYear() != 0 || clock.getHour() != 0 || clock.getMinute() != 0;
    if (!clockSeeded) {
        PL("[Conduct] Time display: waiting for clock seed");
        return;
    }

    ContextManager::refreshTimeSnapshot();
    const auto &timeCtx = ContextManager::time();
    const char *source = isTimeFetched() ? "ntp" : "fallback";
    PF("[Conduct] Time now: %02u:%02u:%02u (%u-%02u-%02u, %s)\n",
       timeCtx.hour, timeCtx.minute, timeCtx.second,
       static_cast<unsigned>(timeCtx.year),
       timeCtx.month, timeCtx.day,
       source);
}

void StatusBoot::plan() {
    PL("[Conduct][Plan] Status boot sequencing");
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    PL("[Conduct][Plan] Status LED initialized");
    StatusPolicy::configure();
}
