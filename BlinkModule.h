#ifndef BLINK_MODULE_H
#define BLINK_MODULE_H

#include "ModuleBase.h"
#include "Timer.h"

/**
 * @brief Example module that blinks an LED using timer-based architecture
 * 
 * This module demonstrates how to use TimerManager instead of
 * direct millis() or delay() calls for periodic operations.
 */
class BlinkModule : public ModuleBase {
public:
    /**
     * @brief Constructor
     * @param pin LED pin number
     * @param interval Blink interval in milliseconds
     */
    BlinkModule(uint8_t pin, unsigned long interval = 1000);

    /**
     * @brief Initialize the module
     */
    void begin() override;

    /**
     * @brief Update the module (called in main loop)
     */
    void update() override;

    /**
     * @brief Set blink interval
     * @param interval Interval in milliseconds
     */
    void setInterval(unsigned long interval);

    /**
     * @brief Get current blink count
     * @return Number of blinks performed
     */
    unsigned long getBlinkCount() const;

private:
    uint8_t _pin;
    unsigned long _interval;
    Timer _blinkTimer;
    bool _state;
    unsigned long _blinkCount;
};

#endif // BLINK_MODULE_H
