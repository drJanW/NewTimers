#include "BlinkModule.h"

BlinkModule::BlinkModule(uint8_t pin, unsigned long interval)
    : ModuleBase("BlinkModule"),
      _pin(pin),
      _interval(interval),
      _state(false),
      _blinkCount(0) {
}

void BlinkModule::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    timerManager().registerTimer(&_blinkTimer);
}

void BlinkModule::update() {
    if (!isEnabled()) {
        return;
    }

    // Use TimerManager's periodic method instead of checking millis() directly
    if (timerManager().periodic(_blinkTimer, _interval)) {
        _state = !_state;
        digitalWrite(_pin, _state ? HIGH : LOW);
        if (_state) {
            _blinkCount++;
        }
    }
}

void BlinkModule::setInterval(unsigned long interval) {
    _interval = interval;
    _blinkTimer.stop();
}

unsigned long BlinkModule::getBlinkCount() const {
    return _blinkCount;
}
