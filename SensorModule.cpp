#include "SensorModule.h"

SensorModule::SensorModule(unsigned long readInterval)
    : ModuleBase("SensorModule"),
      _readInterval(readInterval),
      _lastReading(0),
      _readIndex(0),
      _readCount(0),
      _initialized(false),
      _debounceActive(false) {
    
    // Initialize readings array
    for (int i = 0; i < 10; i++) {
        _readings[i] = 0;
    }
}

void SensorModule::begin() {
    // Register all timers with the manager
    timerManager().registerTimer(&_readTimer);
    timerManager().registerTimer(&_initTimer);
    timerManager().registerTimer(&_debounceTimer);
    timerManager().registerTimer(&_averageWindowTimer);

    Serial.print(_name);
    Serial.println(": Initializing sensor...");
    
    // Start initialization timeout timer (5 seconds)
    _initTimer.start(5000);
    
    // Try to initialize sensor
    _initialized = initializeSensor();
    
    if (_initialized) {
        Serial.print(_name);
        Serial.println(": Sensor initialized successfully");
        _initTimer.stop();
    }
}

void SensorModule::update() {
    if (!isEnabled()) {
        return;
    }

    // Handle initialization timeout
    if (!_initialized) {
        if (_initTimer.expired()) {
            Serial.print(_name);
            Serial.println(": Sensor initialization timeout!");
            disable(); // Disable module if sensor won't initialize
        }
        return;
    }

    // Periodic sensor reading using timer
    if (timerManager().periodic(_readTimer, _readInterval)) {
        // Start debounce timer
        _debounceActive = true;
        _debounceTimer.start(50); // 50ms debounce
    }

    // Handle debounced reading
    if (_debounceActive && _debounceTimer.expired()) {
        _debounceActive = false;
        
        // Read sensor
        int reading = readSensor();
        _lastReading = reading;
        
        // Update rolling average
        updateAverage(reading);
        
        Serial.print(_name);
        Serial.print(": Reading = ");
        Serial.print(reading);
        Serial.print(", Average = ");
        Serial.println(getAverage());
    }
}

int SensorModule::getLastReading() const {
    return _lastReading;
}

float SensorModule::getAverage() const {
    if (_readCount == 0) {
        return 0.0f;
    }
    
    int sum = 0;
    int count = (_readCount < 10) ? _readCount : 10;
    
    for (int i = 0; i < count; i++) {
        sum += _readings[i];
    }
    
    return (float)sum / count;
}

bool SensorModule::isInitialized() const {
    return _initialized;
}

bool SensorModule::initializeSensor() {
    // Simulate sensor initialization
    // In real code, this would configure hardware
    
    // Use timer manager for timing instead of delay()
    Timer initDelay;
    timerManager().registerTimer(&initDelay);
    initDelay.start(100);
    
    while (!initDelay.expired()) {
        // Wait for initialization
    }
    
    timerManager().unregisterTimer(&initDelay);
    return true;
}

int SensorModule::readSensor() {
    // Simulate sensor reading
    // In real code, this would read actual sensor hardware
    
    // Generate simulated sensor data based on time
    // This varies over time to demonstrate the rolling average
    unsigned long time = timerManager().now();
    return (time % 100) + ((time / 1000) % 50);
}

void SensorModule::updateAverage(int newReading) {
    _readings[_readIndex] = newReading;
    _readIndex = (_readIndex + 1) % 10;
    
    if (_readCount < 10) {
        _readCount++;
    }
}
