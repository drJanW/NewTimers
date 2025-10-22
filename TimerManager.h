#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>
#include "Timer.h"

/**
 * @brief Maximum number of managed timers
 */
#define MAX_TIMERS 16

/**
 * @brief Centralized timer management system
 * 
 * This class manages multiple software timers and provides
 * centralized time-related functions to avoid direct use of
 * millis() throughout the codebase.
 */
class TimerManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the TimerManager instance
     */
    static TimerManager& getInstance();

    /**
     * @brief Update all managed timers (call in main loop)
     */
    void update();

    /**
     * @brief Register a timer with the manager
     * @param timer Pointer to the timer to register
     * @return true if registration successful, false if no slots available
     */
    bool registerTimer(Timer* timer);

    /**
     * @brief Unregister a timer from the manager
     * @param timer Pointer to the timer to unregister
     */
    void unregisterTimer(Timer* timer);

    /**
     * @brief Get current time in milliseconds
     * @return Current time from millis()
     */
    unsigned long now() const;

    /**
     * @brief Non-blocking delay using a timer
     * @param timer Timer to use for the delay
     * @param delayMs Delay duration in milliseconds
     * @return true if delay has completed, false otherwise
     */
    bool delay(Timer& timer, unsigned long delayMs);

    /**
     * @brief Create a periodic timer
     * @param timer Timer to use
     * @param periodMs Period in milliseconds
     * @return true if period has elapsed (auto-resets), false otherwise
     */
    bool periodic(Timer& timer, unsigned long periodMs);

private:
    TimerManager();
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;

    Timer* _timers[MAX_TIMERS];
    int _timerCount;
};

#endif // TIMER_MANAGER_H
