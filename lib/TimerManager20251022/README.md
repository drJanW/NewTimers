# TimerManager Library

TimerManager is a lightweight timer scheduler for Arduino-based ESP32 projects.
It allocates up to 16 software timers that fire callbacks at fixed intervals.

- **Non-blocking**: `update()` polls timers from `loop()` without delaying code.
- **Flexible repeats**: run once, N times, or indefinitely.
- **Callback-based**: timers invoke plain `void()` functions.
- **Diagnostics**: inspect available slots or dump active timers for debugging.

This folder contains the reusable library while `src/main.cpp` demonstrates usage.

## Getting Started

1. Include the header and reference the global instance:

```cpp
#include "TimerManager.h"
extern TimerManager timers;
```

2. Define callback functions (e.g. with your `cb_type` macro) that match
`void callback();`.

3. Create timers in `setup()` (or wherever appropriate) and repeatedly call
`timers.update();` inside `loop()`.

## API Overview

```cpp
bool create(uint32_t intervalMs, int32_t repeat, TimerCallback cb);
void cancel(TimerCallback cb);
void update();              // call frequently (usually once per loop)
void showAvailableTimers(bool showAlways);
void dump();                // detailed listing of active slots
```

- `intervalMs`: firing period in milliseconds
- `repeat`: number of executions (0 = infinite; 1 = one-shot; N = N repeats)
- `cb`: pointer to a function with signature `void callback();`

## Examples

### 1. One-Shot Notification

```cpp
static void cb_oneShot()
{
	Serial.println("One-shot!" );
}

void setup()
{
	Serial.begin(115200);
	timers.create(5000, 1, cb_oneShot);  // fire once after 5 seconds
}

void loop()
{
	timers.update();
}
```

### 2. Limited Repeated Task

```cpp
static uint8_t remaining = 3;

static void cb_threeTimes()
{
	Serial.printf("Remaining: %u\n", remaining);
	if (remaining == 0) {
		Serial.println("Done!");
	}
}

void setup()
{
	Serial.begin(115200);
	timers.create(2000, remaining + 1, cb_threeTimes); // 3 repeats + final message
}

void loop()
{
	timers.update();
}
```

### 3. Infinite Heartbeat with Cancel

```cpp
static void cb_heartbeat()
{
	Serial.println("tick");
}

void setup()
{
	Serial.begin(115200);
	timers.create(1000, 0, cb_heartbeat);  // infinite repeats
}

void loop()
{
	static bool stopped = false;
	if (!stopped && millis() > 10000) {
		timers.cancel(cb_heartbeat);
		stopped = true;
	}
	timers.update();
}
```

### 4. Rescheduling Within a Callback

```cpp
static uint32_t interval = 500;

static void cb_accelerate()
{
	Serial.printf("Interval: %lu ms\n", interval);
	interval = max<uint32_t>(50, interval - 50);
	timers.cancel(cb_accelerate);
	timers.create(interval, 0, cb_accelerate);
}

void setup()
{
	Serial.begin(115200);
	timers.create(interval, 0, cb_accelerate);
}

void loop()
{
	timers.update();
}
```

### 5. Diagnostics Helpers

```cpp
void setup()
{
	Serial.begin(115200);
	timers.create(1000, 0, cb_secondTick);
	timers.create(30000, 0, cb_availableTimers); // prints free slots when triggered
}

void loop()
{
	timers.update();

	// Optionally dump active timers on demand
	if (Serial.available() && Serial.read() == 'd') {
		timers.dump();
	}
}
```

## Best Practices

- Keep callbacks short—heavy work belongs elsewhere.
- Avoid blocking delays inside callbacks; they run in the main loop context.
- Reuse callbacks sparingly: the manager enforces one timer per callback pointer.
- Track `bool` flags if you need to know whether `create()` succeeded.
- Call `update()` as often as possible; typically once each iteration of `loop()`.

## Troubleshooting

- **Timer not firing**: ensure the callback pointer is unique and `update()` is invoked.
- **No free slots**: `showAvailableTimers(true)` helps log remaining capacity.
- **Need captures/state**: store state in globals or `static` variables; callbacks must be `void()` functions.
- **Mixed file paths**: The library folder is dated `TimerManager20251018`; update include paths accordingly if you copy it elsewhere.

## License

See the root project for overall licensing. This library follows the same terms unless otherwise noted.
