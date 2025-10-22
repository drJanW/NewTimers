#include "TimerManager.h"

TimerManager::TimerManager() : _timerCount(0) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        _timers[i] = nullptr;
    }
}

TimerManager& TimerManager::getInstance() {
    static TimerManager instance;
    return instance;
}

void TimerManager::update() {
    // Update is called regularly but individual timers handle their own timing
    // This method can be extended for additional timer management tasks
}

bool TimerManager::registerTimer(Timer* timer) {
    if (_timerCount >= MAX_TIMERS) {
        return false;
    }
    
    // Check if already registered
    for (int i = 0; i < _timerCount; i++) {
        if (_timers[i] == timer) {
            return true; // Already registered
        }
    }
    
    _timers[_timerCount++] = timer;
    return true;
}

void TimerManager::unregisterTimer(Timer* timer) {
    for (int i = 0; i < _timerCount; i++) {
        if (_timers[i] == timer) {
            // Shift remaining timers down
            for (int j = i; j < _timerCount - 1; j++) {
                _timers[j] = _timers[j + 1];
            }
            _timers[--_timerCount] = nullptr;
            break;
        }
    }
}

unsigned long TimerManager::now() const {
    return millis();
}

bool TimerManager::delay(Timer& timer, unsigned long delayMs) {
    if (!timer.isRunning()) {
        timer.start(delayMs);
        return false;
    }
    
    if (timer.expired()) {
        timer.stop();
        return true;
    }
    
    return false;
}

bool TimerManager::periodic(Timer& timer, unsigned long periodMs) {
    if (!timer.isRunning()) {
        timer.start(periodMs);
        return false;
    }
    
    if (timer.expired()) {
        timer.reset();
        return true;
    }
    
    return false;
}
