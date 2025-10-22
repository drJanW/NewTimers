#include "Timer.h"

Timer::Timer() : _startTime(0), _duration(0), _running(false) {
}

void Timer::start(unsigned long durationMs) {
    _startTime = millis();
    _duration = durationMs;
    _running = true;
}

void Timer::stop() {
    _running = false;
}

void Timer::reset() {
    if (_running) {
        _startTime = millis();
    }
}

bool Timer::expired() const {
    if (!_running) {
        return false;
    }
    return (millis() - _startTime) >= _duration;
}

bool Timer::isRunning() const {
    return _running;
}

unsigned long Timer::elapsed() const {
    if (!_running) {
        return 0;
    }
    return millis() - _startTime;
}

unsigned long Timer::remaining() const {
    if (!_running) {
        return 0;
    }
    unsigned long elapsedTime = millis() - _startTime;
    if (elapsedTime >= _duration) {
        return 0;
    }
    return _duration - elapsedTime;
}

unsigned long Timer::duration() const {
    return _duration;
}
