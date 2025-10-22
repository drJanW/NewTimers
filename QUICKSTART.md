# Quick Start Guide

## Getting Started with NewTimers

This guide will help you get started with the timer-managed architecture.

## Installation

1. Clone or download this repository
2. Open `NewTimers.ino` in Arduino IDE
3. Select your board and port
4. Upload to your Arduino

## Basic Usage

### 1. Create a Simple Timer

```cpp
#include "Timer.h"
#include "TimerManager.h"

Timer myTimer;
TimerManager& tm = TimerManager::getInstance();

void setup() {
    tm.registerTimer(&myTimer);
    myTimer.start(1000); // 1 second timer
}

void loop() {
    if (myTimer.expired()) {
        // Timer expired - do something
        myTimer.reset(); // Restart for next interval
    }
}
```

### 2. Use Periodic Timer

```cpp
Timer periodicTimer;

void setup() {
    TimerManager::getInstance().registerTimer(&periodicTimer);
}

void loop() {
    // Execute every 500ms
    if (TimerManager::getInstance().periodic(periodicTimer, 500)) {
        Serial.println("Tick!");
    }
}
```

### 3. Non-Blocking Delay

```cpp
Timer delayTimer;

void setup() {
    TimerManager::getInstance().registerTimer(&delayTimer);
}

void loop() {
    // Non-blocking 2 second delay
    if (TimerManager::getInstance().delay(delayTimer, 2000)) {
        Serial.println("2 seconds passed!");
        delayTimer.stop(); // Reset for next use
    }
}
```

### 4. Create Your Own Module

```cpp
#include "ModuleBase.h"
#include "Timer.h"

class MyModule : public ModuleBase {
private:
    Timer _updateTimer;
    int _counter;

public:
    MyModule() : ModuleBase("MyModule"), _counter(0) {}
    
    void begin() override {
        timerManager().registerTimer(&_updateTimer);
        Serial.println("MyModule started");
    }
    
    void update() override {
        if (!isEnabled()) return;
        
        // Update every 1 second
        if (timerManager().periodic(_updateTimer, 1000)) {
            _counter++;
            Serial.print("Counter: ");
            Serial.println(_counter);
        }
    }
};
```

### 5. Use Your Module

```cpp
#include "TimerManager.h"
#include "MyModule.h"

MyModule myModule;

void setup() {
    Serial.begin(115200);
    myModule.begin();
}

void loop() {
    TimerManager::getInstance().update();
    myModule.update();
}
```

## Migration from Old Code

### Before (❌ Old way)
```cpp
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000;

void loop() {
    if (millis() - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = millis();
        doUpdate();
    }
}
```

### After (✅ New way)
```cpp
Timer updateTimer;
TimerManager& tm = TimerManager::getInstance();

void setup() {
    tm.registerTimer(&updateTimer);
}

void loop() {
    if (tm.periodic(updateTimer, 1000)) {
        doUpdate();
    }
}
```

## Common Patterns

### Pattern 1: Timeout
```cpp
Timer timeoutTimer;

void startOperation() {
    timeoutTimer.start(5000); // 5 second timeout
}

void loop() {
    if (timeoutTimer.isRunning() && timeoutTimer.expired()) {
        Serial.println("Operation timed out!");
        timeoutTimer.stop();
    }
}
```

### Pattern 2: Retry Logic
```cpp
Timer retryTimer;
int attempts = 0;
const int MAX_ATTEMPTS = 3;

void loop() {
    if (attempts < MAX_ATTEMPTS) {
        if (!retryTimer.isRunning()) {
            retryTimer.start(1000); // Try every second
        }
        
        if (retryTimer.expired()) {
            attempts++;
            if (tryOperation()) {
                attempts = MAX_ATTEMPTS; // Success, stop trying
            }
            retryTimer.stop(); // Reset for next attempt
        }
    }
}
```

### Pattern 3: Debouncing
```cpp
Timer debounceTimer;
bool buttonPressed = false;

void loop() {
    bool currentState = digitalRead(BUTTON_PIN);
    
    if (currentState && !buttonPressed && !debounceTimer.isRunning()) {
        debounceTimer.start(50); // 50ms debounce
    }
    
    if (debounceTimer.expired()) {
        if (digitalRead(BUTTON_PIN)) {
            buttonPressed = true;
            handleButtonPress();
        }
        debounceTimer.stop();
    }
    
    if (!currentState) {
        buttonPressed = false;
    }
}
```

### Pattern 4: State Machine
```cpp
enum State { IDLE, WORKING, DONE };
State currentState = IDLE;
Timer stateTimer;

void loop() {
    switch (currentState) {
        case IDLE:
            if (shouldStart()) {
                stateTimer.start(2000);
                currentState = WORKING;
            }
            break;
            
        case WORKING:
            if (stateTimer.expired()) {
                currentState = DONE;
            }
            break;
            
        case DONE:
            // Handle completion
            currentState = IDLE;
            break;
    }
}
```

## Tips and Best Practices

1. **Always register timers** with TimerManager in your `begin()` or `setup()` method
2. **Use `timerManager().now()`** instead of direct `millis()` calls
3. **Prefer `periodic()`** for repeating tasks
4. **Use `delay()`** for one-time delays
5. **Stop timers** when you're done with them to save resources
6. **Extend ModuleBase** for new modules to get automatic TimerManager access
7. **Check `isEnabled()`** in your update methods to respect module state

## Examples Included

1. **BlinkModule** - Simple LED blinking
2. **RetryModule** - Retry with attempts and delays
3. **SensorModule** - Multi-timer sensor reading with debouncing
4. **StateMachineModule** - State machine with timeouts

See the source files for complete implementations!

## Troubleshooting

**Timer doesn't fire:**
- Make sure you registered it with `timerManager().registerTimer(&timer)`
- Check that you called `timer.start(duration)` before checking `expired()`

**Module doesn't update:**
- Verify you're calling `module.update()` in the main loop
- Check if module is enabled with `module.isEnabled()`

**Timing is inaccurate:**
- Remember that timer resolution depends on how often you call `update()` or check `expired()`
- For critical timing, check timers more frequently in your main loop

## Next Steps

- Read `ARCHITECTURE.md` for design rationale
- Review example modules to understand patterns
- Create your own modules extending `ModuleBase`
- Replace old timing code with timer-based architecture

Happy coding with timer-managed modules!
