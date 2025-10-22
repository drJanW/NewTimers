#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

/**
 * @brief Software timer class for non-blocking time management
 * 
 * This class provides a software timer that can be used for delays,
 * periodic events, and timeouts without blocking program execution.
 */
class Timer {
public:
    /**
     * @brief Constructor
     */
    Timer();

    /**
     * @brief Start or restart the timer with a specified duration
     * @param durationMs Duration in milliseconds
     */
    void start(unsigned long durationMs);

    /**
     * @brief Stop the timer
     */
    void stop();

    /**
     * @brief Reset the timer (restart with same duration)
     */
    void reset();

    /**
     * @brief Check if the timer has expired
     * @return true if timer has expired, false otherwise
     */
    bool expired() const;

    /**
     * @brief Check if the timer is running
     * @return true if timer is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Get the elapsed time since timer start
     * @return Elapsed time in milliseconds
     */
    unsigned long elapsed() const;

    /**
     * @brief Get the remaining time until expiration
     * @return Remaining time in milliseconds (0 if expired)
     */
    unsigned long remaining() const;

    /**
     * @brief Get the timer duration
     * @return Duration in milliseconds
     */
    unsigned long duration() const;

private:
    unsigned long _startTime;
    unsigned long _duration;
    bool _running;
};

#endif // TIMER_H
