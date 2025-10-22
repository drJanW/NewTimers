#ifndef SENSOR_MODULE_H
#define SENSOR_MODULE_H

#include "ModuleBase.h"
#include "Timer.h"

/**
 * @brief Example sensor module demonstrating complex timer usage
 * 
 * This module shows how to use multiple timers for different purposes:
 * - Reading sensor data periodically
 * - Timeout for sensor initialization
 * - Debouncing sensor readings
 * - Data averaging window
 * 
 * All timing is managed through TimerManager, no local timing variables.
 */
class SensorModule : public ModuleBase {
public:
    /**
     * @brief Constructor
     * @param readInterval Interval between sensor reads in milliseconds
     */
    SensorModule(unsigned long readInterval = 1000);

    /**
     * @brief Initialize the module
     */
    void begin() override;

    /**
     * @brief Update the module (called in main loop)
     */
    void update() override;

    /**
     * @brief Get the last sensor reading
     * @return Last sensor value
     */
    int getLastReading() const;

    /**
     * @brief Get average of recent readings
     * @return Average sensor value
     */
    float getAverage() const;

    /**
     * @brief Check if sensor is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;

private:
    unsigned long _readInterval;
    Timer _readTimer;           // Periodic sensor reading
    Timer _initTimer;           // Initialization timeout
    Timer _debounceTimer;       // Debounce timer
    Timer _averageWindowTimer;  // Rolling average window
    
    int _lastReading;
    int _readings[10];
    int _readIndex;
    int _readCount;
    bool _initialized;
    bool _debounceActive;

    /**
     * @brief Initialize sensor hardware
     * @return true if successful
     */
    bool initializeSensor();

    /**
     * @brief Read sensor value
     * @return Sensor reading
     */
    int readSensor();

    /**
     * @brief Update rolling average
     */
    void updateAverage(int newReading);
};

#endif // SENSOR_MODULE_H
