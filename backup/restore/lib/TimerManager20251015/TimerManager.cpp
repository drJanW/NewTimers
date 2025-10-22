#include "TimerManager.h"

TimerManager timers(MAX_TIMERS);  // global instance

TimerManager::TimerManager(uint8_t maxTimers) {
  capacity = maxTimers;
  timers = new Timer[capacity];
  for (uint8_t i = 0; i < capacity; ++i) {
    timers[i].callback = nullptr;
  }
}

int TimerManager::find(Callback callback) const {
  for (uint8_t i = 0; i < capacity; ++i) {
    if (timers[i].callback == callback) return i;
  }
  return -1;
}

int TimerManager::findFreeSlot() const {
  for (uint8_t i = 0; i < capacity; ++i) {
    if (timers[i].callback == nullptr) return i;
  }
  return -1;
}

bool TimerManager::create(uint32_t duration, uint16_t repeat, Callback callback) {
  if (duration < MIN_INTERVAL_MS) {
#if TIMER_DEBUG
    Serial.printf("TimerManager: interval %u ms too short (min %u)\n", duration, MIN_INTERVAL_MS);
#endif
    duration = MIN_INTERVAL_MS;
  }

  if (find(callback) != -1) {
#if TIMER_DEBUG
    Serial.println("TimerManager: creation failed - callback already in use");
#endif
    return false;
  }

  int slot = findFreeSlot();
  if (slot == -1) {
#if TIMER_DEBUG
    Serial.println("TimerManager: creation failed - no free slots");
#endif
    return false;
  }

  Timer& t = timers[slot];
  t.duration = duration;
  t.elapsed = 0;
  t.startTime = millis();
  t.repeat = repeat;
  t.callback = callback;
  t.paused = false;

  return true;
}

bool TimerManager::cancel(Callback callback) {
  int i = find(callback);
  if (i == -1) return false;
  timers[i].callback = nullptr;
  return true;
}

bool TimerManager::pause(Callback callback) {
  int i = find(callback);
  if (i == -1 || timers[i].paused) return false;
  timers[i].elapsed += millis() - timers[i].startTime;
  timers[i].paused = true;
  return true;
}

bool TimerManager::resume(Callback callback) {
  int i = find(callback);
  if (i == -1 || !timers[i].paused) return false;
  timers[i].startTime = millis();
  timers[i].paused = false;
  return true;
}

bool TimerManager::reset(Callback callback) {
  int i = find(callback);
  if (i == -1) return false;
  timers[i].elapsed = 0;
  timers[i].startTime = millis();
  return true;
}

bool TimerManager::isActive(Callback callback) const {
  int i = find(callback);
  return i != -1 && !timers[i].paused;
}

bool TimerManager::isPaused(Callback callback) const {
  int i = find(callback);
  return i != -1 && timers[i].paused;
}

void TimerManager::update() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < capacity; ++i) {
    Timer& t = timers[i];
    if (t.callback == nullptr || t.paused) continue;

    uint32_t total = t.elapsed + (now - t.startTime);
    if (total >= t.duration) {
      t.callback();

      if (t.repeat == 1) {
        t.callback = nullptr;
      } else {
        if (t.repeat > 1) t.repeat--;
        t.elapsed = 0;
        t.startTime = now;
      }
    }
  }
}