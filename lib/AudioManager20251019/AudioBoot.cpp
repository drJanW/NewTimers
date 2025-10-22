#include "AudioBoot.h"
#include "TimerManager.h"
#include "ConductManager.h"

// Schedule recurring audio-related tasks (sayTime, playFragment)
void audioBoot_plan() {
    TimerManager &timers = TimerManager::instance();
    timers.create(3600000, 0, [](){ ConductManager::intentSayTime(); });      // hourly
    timers.create(600000, 0, [](){ ConductManager::intentPlayFragment(); });  // every 10 min
}
