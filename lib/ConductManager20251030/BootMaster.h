
#ifndef BOOTMASTER_H
#define BOOTMASTER_H
#include <cstdint>

class BootMaster {
public:
    bool begin();

private:
    bool ensureTimer();
    void bootstrapTick();
    static void timerThunk();
    static void fallbackThunk();
    bool ensureFallbackTimer();
    void cancelFallbackTimer();
    void fallbackTimeout();

    struct FallbackStatus {
        bool timerArmed = false;
        bool seedAttempted = false;
        bool seededFromCache = false;
        bool stateAnnounced = false;

        void resetFlags() {
            seedAttempted = false;
            seededFromCache = false;
            stateAnnounced = false;
        }

        void resetAll() {
            timerArmed = false;
            resetFlags();
        }
    };

    bool timerArmed = false;
    FallbackStatus fallback;
};

extern BootMaster bootMaster;

#endif // BOOTMASTER_H
