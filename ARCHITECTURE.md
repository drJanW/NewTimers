# Architecture Comparison

## Before: Traditional Approach (What NOT to do)

### Old Style Module with Local Timing Variables

```cpp
class OldStyleModule {
private:
    unsigned long lastBlinkTime;
    unsigned long lastRetryTime;
    int attempts;
    unsigned long delayMs;
    
public:
    OldStyleModule() : lastBlinkTime(0), lastRetryTime(0), attempts(0), delayMs(1000) {}
    
    void update() {
        // Direct millis() usage scattered throughout code
        unsigned long now = millis();
        
        // Blink LED logic
        if (now - lastBlinkTime >= 500) {
            lastBlinkTime = now;
            toggleLED();
        }
        
        // Retry logic with local timing
        if (now - lastRetryTime >= delayMs) {
            lastRetryTime = now;
            attempts++;
            if (attempts < 5) {
                retryOperation();
            }
        }
    }
};
```

**Problems:**
- Multiple `millis()` calls throughout the code
- Local timing variables (`lastBlinkTime`, `lastRetryTime`, `delayMs`, `attempts`)
- Difficult to test and maintain
- Timing logic mixed with business logic
- No centralized time management

## After: Timer-Managed Architecture (Current Implementation)

### New Style Module with TimerManager

```cpp
class NewStyleModule : public ModuleBase {
private:
    Timer _blinkTimer;
    Timer _retryTimer;
    
public:
    NewStyleModule() : ModuleBase("NewStyleModule") {}
    
    void begin() override {
        timerManager().registerTimer(&_blinkTimer);
        timerManager().registerTimer(&_retryTimer);
    }
    
    void update() override {
        if (!isEnabled()) return;
        
        // Blink LED using periodic timer
        if (timerManager().periodic(_blinkTimer, 500)) {
            toggleLED();
        }
        
        // Retry logic using timer-based delay
        if (timerManager().delay(_retryTimer, 1000)) {
            retryOperation();
            _retryTimer.stop();  // Reset for next use
        }
    }
};
```

**Benefits:**
- No direct `millis()` calls in module code
- No local timing variables
- Clean separation of timing logic and business logic
- Centralized time management through `TimerManager`
- Easy to test and maintain
- Timers can be reused for different operations

## Key Differences

| Aspect | Old Approach | New Approach |
|--------|-------------|--------------|
| Timing Source | Local `millis()` calls | `TimerManager::now()` |
| State Storage | Local variables (`lastTime`, `attempts`) | `Timer` objects |
| Delay Handling | `if (millis() - lastTime > delay)` | `timerManager().delay(timer, ms)` |
| Periodic Events | Manual time tracking | `timerManager().periodic(timer, ms)` |
| Testing | Difficult (real time dependent) | Easy (TimerManager can be mocked) |
| Maintenance | Scattered timing logic | Centralized in TimerManager |

## Migration Example

### Before

```cpp
unsigned long lastUpdate = 0;
int attemptCount = 0;
const unsigned long UPDATE_INTERVAL = 1000;

void loop() {
    if (millis() - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = millis();
        attemptCount++;
        doSomething();
    }
}
```

### After

```cpp
Timer updateTimer;
TimerManager& tm = TimerManager::getInstance();

void setup() {
    tm.registerTimer(&updateTimer);
}

void loop() {
    if (tm.periodic(updateTimer, 1000)) {
        doSomething();
    }
}
```

## Refactoring Checklist

When converting old code to the new architecture:

- [ ] Replace all `millis()` calls with `timerManager().now()`
- [ ] Replace `lastTime` variables with `Timer` objects
- [ ] Replace manual delay checking with `timerManager().delay()`
- [ ] Replace periodic time checks with `timerManager().periodic()`
- [ ] Remove local timing variables (`attempts`, `delayMs`, etc.)
- [ ] Extend `ModuleBase` instead of creating standalone classes
- [ ] Register timers in `begin()` method
- [ ] Use `timerManager()` protected method to access TimerManager

## Why This Matters

The refactored architecture provides:

1. **Single Source of Truth**: All time comes from TimerManager
2. **Testability**: Mock TimerManager for unit tests
3. **Debugging**: Centralized logging of all timer events
4. **Optimization**: TimerManager can optimize timer resolution
5. **Consistency**: Same patterns used throughout the codebase
6. **Power Management**: Future support for sleep modes through TimerManager
