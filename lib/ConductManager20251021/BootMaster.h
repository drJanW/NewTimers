
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

    bool timerArmed = false;
    uint32_t waitStartMs = 0;
    bool fallbackSeedAttempted = false;
    bool fallbackSeededFromCache = false;
    bool fallbackStateAnnounced = false;
};

extern BootMaster bootMaster;

#endif // BOOTMASTER_H
