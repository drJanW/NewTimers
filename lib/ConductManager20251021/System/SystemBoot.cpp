#include "SystemBoot.h"
#include "TimerManager.h"
#include "ConductManager.h"

static TimerCallback heartbeatCb = [](){ ConductManager::intentPulseStatusLed(); };
static uint32_t heartbeatInterval = 500;

void systemBoot_plan() {
    TimerManager &timers = TimerManager::instance();
    timers.create(heartbeatInterval, 0, heartbeatCb);
}
