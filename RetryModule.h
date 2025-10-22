#ifndef RETRY_MODULE_H
#define RETRY_MODULE_H

#include "ModuleBase.h"
#include "Timer.h"

/**
 * @brief Example module demonstrating retry logic with timer-based delays
 * 
 * This module shows how to implement retry attempts without using
 * local millis() or attempt counters, instead using TimerManager.
 */
class RetryModule : public ModuleBase {
public:
    /**
     * @brief Constructor
     * @param maxAttempts Maximum number of retry attempts
     * @param retryDelay Delay between retries in milliseconds
     */
    RetryModule(uint8_t maxAttempts = 3, unsigned long retryDelay = 1000);

    /**
     * @brief Initialize the module
     */
    void begin() override;

    /**
     * @brief Update the module (called in main loop)
     */
    void update() override;

    /**
     * @brief Start an operation that may need retries
     */
    void startOperation();

    /**
     * @brief Check if operation is complete
     * @return true if operation completed successfully or max attempts reached
     */
    bool isComplete() const;

    /**
     * @brief Check if operation succeeded
     * @return true if operation succeeded, false if failed
     */
    bool wasSuccessful() const;

    /**
     * @brief Get current attempt number
     * @return Current attempt (1-based)
     */
    uint8_t getCurrentAttempt() const;

    /**
     * @brief Reset the module for a new operation
     */
    void reset();

private:
    uint8_t _maxAttempts;
    unsigned long _retryDelay;
    uint8_t _currentAttempt;
    Timer _retryTimer;
    bool _operationActive;
    bool _operationSuccess;

    /**
     * @brief Simulate an operation that may fail
     * @return true if operation succeeds, false otherwise
     */
    bool performOperation();
};

#endif // RETRY_MODULE_H
