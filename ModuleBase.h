#ifndef MODULE_BASE_H
#define MODULE_BASE_H

#include <Arduino.h>
#include "TimerManager.h"

/**
 * @brief Base class for all managed modules
 * 
 * This class provides the foundation for layered, managed modules
 * that use TimerManager for all timing operations instead of
 * direct calls to millis() or delay().
 */
class ModuleBase {
public:
    /**
     * @brief Constructor
     * @param name Module name for identification
     */
    ModuleBase(const char* name);

    /**
     * @brief Virtual destructor
     */
    virtual ~ModuleBase();

    /**
     * @brief Initialize the module
     * Called once during setup
     */
    virtual void begin();

    /**
     * @brief Update the module
     * Called repeatedly in the main loop
     */
    virtual void update() = 0;

    /**
     * @brief Get the module name
     * @return Module name string
     */
    const char* getName() const;

    /**
     * @brief Check if module is enabled
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const;

    /**
     * @brief Enable the module
     */
    void enable();

    /**
     * @brief Disable the module
     */
    void disable();

protected:
    /**
     * @brief Get reference to the timer manager
     * @return Reference to TimerManager singleton
     */
    TimerManager& timerManager();

    const char* _name;
    bool _enabled;
};

#endif // MODULE_BASE_H
