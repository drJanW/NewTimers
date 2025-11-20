#pragma once

class PRTClock;

class ClockConduct {
public:
    void plan();

    static bool seedClockFromRtc(PRTClock &clock);
    static void syncRtcFromClock(const PRTClock &clock);
    static bool hasRtc();
};
