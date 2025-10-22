# NewTimers

A refactored Arduino project with a layered managed module architecture using centralized timer management.

## Overview

This project demonstrates a modern architecture for Arduino/embedded systems where all timing operations are managed through a centralized `TimerManager` instead of scattered `millis()`, `delay()`, or local timing variables throughout the code.

## Architecture

### Core Components

1. **Timer** - Individual software timer for non-blocking time management
2. **TimerManager** - Singleton that manages all timers and provides centralized time functions
3. **ModuleBase** - Base class for all managed modules providing common functionality
4. **Example Modules** - Demonstrate the architecture in action

### Key Features

- ✅ **No Local Timing Variables**: Modules don't maintain local `millis()`, `now`, `attempts`, or `delayMs` variables
- ✅ **Centralized Time Management**: All timing goes through `TimerManager`
- ✅ **Non-blocking Operations**: Software timers enable concurrent operations without blocking
- ✅ **Layered Architecture**: Clean separation of concerns with base classes and modules
- ✅ **Reusable Timers**: Timer instances can be reused for different operations

## API Reference

### Timer Class

```cpp
Timer myTimer;

// Start timer with duration
myTimer.start(1000);  // 1 second

// Check if expired
if (myTimer.expired()) {
  // Timer has expired
}

// Reset timer (restart with same duration)
myTimer.reset();

// Get elapsed/remaining time
unsigned long elapsed = myTimer.elapsed();
unsigned long remaining = myTimer.remaining();

// Stop timer
myTimer.stop();
```

### TimerManager Class

```cpp
TimerManager& tm = TimerManager::getInstance();

// Get current time (replaces direct millis() calls)
unsigned long now = tm.now();

// Non-blocking delay
Timer delayTimer;
if (tm.delay(delayTimer, 5000)) {
  // 5 seconds have passed
}

// Periodic timer (auto-resets)
Timer periodicTimer;
if (tm.periodic(periodicTimer, 1000)) {
  // Executes every 1 second
}

// Register/unregister timers
tm.registerTimer(&myTimer);
tm.unregisterTimer(&myTimer);
```

### ModuleBase Class

```cpp
class MyModule : public ModuleBase {
public:
  MyModule() : ModuleBase("MyModule") {}
  
  void begin() override {
    // Initialize module
    timerManager().registerTimer(&_timer);
  }
  
  void update() override {
    if (!isEnabled()) return;
    
    // Use timerManager() instead of direct millis()
    if (timerManager().periodic(_timer, 1000)) {
      // Do something every second
    }
  }

private:
  Timer _timer;
};
```

## Example Modules

### BlinkModule

Blinks an LED using timer-based periodic execution instead of `millis()` tracking:

```cpp
BlinkModule ledBlink(LED_BUILTIN, 500);  // Blink every 500ms

void setup() {
  ledBlink.begin();
}

void loop() {
  ledBlink.update();
}
```

### RetryModule

Demonstrates retry logic with configurable attempts and delays, all managed through timers:

```cpp
RetryModule retry(5, 2000);  // 5 attempts, 2 second delay

void setup() {
  retry.begin();
  retry.startOperation();
}

void loop() {
  retry.update();
  
  if (retry.isComplete()) {
    if (retry.wasSuccessful()) {
      // Operation succeeded
    } else {
      // All attempts failed
    }
  }
}
```

## Benefits of This Architecture

1. **Maintainability**: Centralized timing logic makes code easier to maintain and debug
2. **Testability**: Timer behavior can be mocked/stubbed for unit testing
3. **Consistency**: Uniform timing approach across all modules
4. **Scalability**: Easy to add new timer-based features without duplicating timing logic
5. **Resource Management**: TimerManager can track and optimize timer usage

## Building and Running

This is an Arduino sketch. To use it:

1. Open `NewTimers.ino` in Arduino IDE
2. Select your board and port
3. Upload to your Arduino
4. Open Serial Monitor (115200 baud) to see the demo output

## File Structure

```
NewTimers/
├── NewTimers.ino          # Main sketch
├── Timer.h                # Timer class header
├── Timer.cpp              # Timer class implementation
├── TimerManager.h         # TimerManager class header
├── TimerManager.cpp       # TimerManager class implementation
├── ModuleBase.h           # Base module class header
├── ModuleBase.cpp         # Base module class implementation
├── BlinkModule.h          # Example blink module header
├── BlinkModule.cpp        # Example blink module implementation
├── RetryModule.h          # Example retry module header
├── RetryModule.cpp        # Example retry module implementation
└── README.md              # This file
```

## License

This project is provided as-is for educational and reference purposes.
