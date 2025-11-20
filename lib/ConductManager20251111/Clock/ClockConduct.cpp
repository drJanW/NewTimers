#include "ClockConduct.h"

#include "ClockPolicy.h"
#include "PRTClock.h"
#include "Globals.h"

void ClockConduct::plan() {
    if (ClockPolicy::isRtcAvailable()) {
        PL("[Conduct][Plan] RTC conduct ready (fallback + sync)");
    } else {
        PL("[Conduct][Plan] RTC hardware not detected");
    }
}

bool ClockConduct::seedClockFromRtc(PRTClock &clock) {
    return ClockPolicy::seedClockFromRTC(clock);
}

void ClockConduct::syncRtcFromClock(const PRTClock &clock) {
    ClockPolicy::syncRTCFromClock(clock);
}

bool ClockConduct::hasRtc() {
    return ClockPolicy::isRtcAvailable();
}
