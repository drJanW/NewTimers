#include "RetryModule.h"

RetryModule::RetryModule(uint8_t maxAttempts, unsigned long retryDelay)
    : ModuleBase("RetryModule"),
      _maxAttempts(maxAttempts),
      _retryDelay(retryDelay),
      _currentAttempt(0),
      _operationActive(false),
      _operationSuccess(false) {
}

void RetryModule::begin() {
    timerManager().registerTimer(&_retryTimer);
}

void RetryModule::update() {
    if (!isEnabled() || !_operationActive) {
        return;
    }

    // Wait for retry delay if we're not on first attempt
    if (_currentAttempt > 0 && !timerManager().delay(_retryTimer, _retryDelay)) {
        return; // Still waiting for retry delay
    }

    // Perform the operation
    _currentAttempt++;
    _operationSuccess = performOperation();

    if (_operationSuccess) {
        _operationActive = false;
        Serial.print(_name);
        Serial.print(": Operation succeeded on attempt ");
        Serial.println(_currentAttempt);
    } else if (_currentAttempt >= _maxAttempts) {
        _operationActive = false;
        Serial.print(_name);
        Serial.print(": Operation failed after ");
        Serial.print(_currentAttempt);
        Serial.println(" attempts");
    } else {
        // Prepare for next retry
        _retryTimer.stop(); // Reset timer for next delay
        Serial.print(_name);
        Serial.print(": Attempt ");
        Serial.print(_currentAttempt);
        Serial.print(" failed, retrying in ");
        Serial.print(_retryDelay);
        Serial.println("ms");
    }
}

void RetryModule::startOperation() {
    reset();
    _operationActive = true;
}

bool RetryModule::isComplete() const {
    return !_operationActive;
}

bool RetryModule::wasSuccessful() const {
    return _operationSuccess;
}

uint8_t RetryModule::getCurrentAttempt() const {
    return _currentAttempt;
}

void RetryModule::reset() {
    _currentAttempt = 0;
    _operationActive = false;
    _operationSuccess = false;
    _retryTimer.stop();
}

bool RetryModule::performOperation() {
    // Simulate an operation with 50% success rate
    // In real code, this would be actual operation logic
    return (timerManager().now() % 2) == 0;
}
