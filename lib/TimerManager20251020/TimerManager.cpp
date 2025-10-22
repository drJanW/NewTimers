// =============================================
// TimerManager.cpp
// =============================================
#include "TimerManager.h"
#include "Globals.h"   // for PF/PL macros

TimerManager& TimerManager::instance() {
    static TimerManager inst;
    return inst;
}

TimerManager::TimerManager() {
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        slots[i].active = false;
    }
}

bool TimerManager::create(uint32_t interval, int32_t repeat, TimerCallback cb) {
    if (!cb) return false;

    // enforce "one callback = one timer"
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (slots[i].active && slots[i].cb == cb) {
#if DEBUG_TIMERS
            PF("TimerManager: creation failed - callback already in use\n");
#endif
            return false;
        }
    }

    // find free slot
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!slots[i].active) {
            slots[i].active = true;
            slots[i].cb = cb;
            slots[i].interval = interval;
            slots[i].nextFire = millis() + interval;
            slots[i].repeat = repeat;
            return true;
        }
    }

#if DEBUG_TIMERS
    PL("TimerManager: no free slots");
#endif
    return false;
}

void TimerManager::cancel(TimerCallback cb) {
    if (!cb) return;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (slots[i].active && slots[i].cb == cb) {
            slots[i].active = false;
            slots[i].cb = nullptr;
            return;
        }
    }
}

void TimerManager::update() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!slots[i].active) continue;
        if ((int32_t)(now - slots[i].nextFire) >= 0) {
            // fire callback
            TimerCallback cb = slots[i].cb;
            if (cb) cb();

            // reschedule or finish
            if (slots[i].repeat == 1) {
                slots[i].active = false;
                slots[i].cb = nullptr;
            } else {
                if (slots[i].repeat > 1) slots[i].repeat--;
                slots[i].nextFire += slots[i].interval;
            }
        }
    }
}

// ===================================================
// Diagnostics
// ===================================================
void TimerManager::showAvailableTimers(bool showAlways) {
    static uint8_t minFree = MAX_TIMERS;
    uint8_t freeCount = 0;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (!slots[i].active) freeCount++;
    }

    if (freeCount < minFree) {
        minFree = freeCount;
        PF("[TimerManager] New minimum: only %d free timers available\n", freeCount);
    }

    if (showAlways) {
        PF("[TimerManager] Free timers now %d (historical min %d)\n", freeCount, minFree);
    }
}

void TimerManager::showTimerCountStatus(bool showAlways) {
    showAvailableTimers(showAlways);
}

void TimerManager::dump() {//TODO: get rid of this unwanted shit
    PL("[TimerManager] Active timers:");
    uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
        if (slots[i].active) {
            PF("  Slot %d: cb=%p interval=%lu ms nextFire in %ld ms repeat=%ld\n",
               i,
               (void*)slots[i].cb,
               (unsigned long)slots[i].interval,
               (long)(slots[i].nextFire - now),
               (long)slots[i].repeat);
        }
    }
}