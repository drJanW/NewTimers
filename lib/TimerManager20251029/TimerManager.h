// =============================================
// TimerManager.h
// =============================================
#pragma once
#include <Arduino.h>

// Type alias for timer callbacks
typedef void (*TimerCallback)();
#define cb_type static void

class TimerManager {
public:
    struct TimerSlot {
        bool active = false;
        TimerCallback cb = nullptr;
        uint32_t interval = 0;   // ms
        uint32_t nextFire = 0;   // millis() when it should fire
        int32_t repeat = 0;      // 0 = infinite, >0 = remaining repeats
    };

    static const uint8_t MAX_TIMERS = 60;

    static TimerManager& instance();

    // Create a timer
    // interval: ms
    // repeat: 0 = infinite, 1 = one-shot, N = N repeats
    // cb: callback function
    // returns true if created, false if failed
    bool create(uint32_t interval, int32_t repeat, TimerCallback cb);

    // Cancel a timer by callback
    void cancel(TimerCallback cb);

    // Restart (or start) a timer by callback
    bool restart(uint32_t interval, int32_t repeat, TimerCallback cb);

    // Update timers (call in loop)
    void update();

    bool isActive(TimerCallback cb) const;

    // Diagnostics
    void showAvailableTimers(bool showAlways);
    void showTimerCountStatus(bool showAlways = false);
    void dump();

private:
    TimerManager();

    TimerSlot slots[MAX_TIMERS];
};