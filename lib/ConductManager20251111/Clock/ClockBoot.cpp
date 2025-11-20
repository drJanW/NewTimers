#include "ClockBoot.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockBoot::plan() {
    ClockPolicy::configureHardware();

    auto &clock = PRTClock::instance();
    (void)ClockPolicy::seedClockFromRTC(clock);
}
