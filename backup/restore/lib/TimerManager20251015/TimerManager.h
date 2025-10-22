#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>

#define MAX_TIMERS 30
#define TIMER_DEBUG 1  // local debug toggle

class TimerManager {
public:
  using Callback = void (*)();

  explicit TimerManager(uint8_t maxTimers);

  bool create(uint32_t duration, uint16_t repeat, Callback callback);
  bool cancel(Callback callback);
  bool pause(Callback callback);
  bool resume(Callback callback);
  bool reset(Callback callback);
  bool isActive(Callback callback) const;
  bool isPaused(Callback callback) const;

  void update();

private:
  struct Timer {
    uint32_t duration;
    uint32_t elapsed;
    uint32_t startTime;
    uint16_t repeat;
    Callback callback;
    bool paused;
  };

  Timer* timers;
  uint8_t capacity;

  static constexpr uint32_t MIN_INTERVAL_MS = 5;

  int find(Callback callback) const;
  int findFreeSlot() const;
};

extern TimerManager timers;

#endif