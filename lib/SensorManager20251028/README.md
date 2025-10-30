# SensorManager

Thin orchestrator for uniform sensor drivers. Timer-driven polling. No ISRs in v1. `loop()` only services `TimerSystem`.

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Events and Data](#events-and-data)
- [Timing](#timing)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [License](#license)

## Overview
`SensorManager` coordinates multiple sensors that implement a **uniform driver API**. Each sensor owns its I/O and buffers; the manager just calls `init()` once and `update()` on a fixed interval, then forwards events and stable samples to consumers (e.g., `ContextManager`).

**Assumptions for v1**
- Signals are ≥ 200 ms, so polling via `TimerSystem` is sufficient.
- No interrupt handlers; ISR-based fast paths can be added later if required.

## Features
- Uniform driver contract for all sensors
- Single timer drives the entire sensing pipeline
- Pull-based model: drivers expose data/events; manager drains them
- Deterministic ordering with a static registry
- No dynamic allocation; one global instance per sensor module
- Internal double buffering per sensor for coherent reads

## Architecture
```
+-----------------+        +-----------------+        +------------------+
|  Sensor Driver  |  ...   |  Sensor Driver  |  ...   |  Sensor Driver   |
|  (owns I/O,FSM) |        |  (owns I/O,FSM) |        |  (owns I/O,FSM)  |
+--------+--------+        +--------+--------+        +---------+--------+
         |                          |                           |
         v                          v                           v
                        +---------------------+
                        |   SensorManager     |
                        |  - registry[]       |
                        |  - timer -> update  |
                        |  - event ring       |
                        |  - data snapshots   |
                        +-----+----------+----+
                              |          |
                              v          v
                     ContextManager   Other consumers
```
Drivers never call back into app logic; the manager **pulls**.

## Installation
- Arduino-ESP32 5.5.1+ (ESP32‑D0WD‑V3 tested)
- Copy `SensorManager.{h,cpp}` and sensor drivers into your project `src/`.
- Ensure `TimerSystem` is available and initialized.

## Configuration
- Poll period: set via `SensorManager::init(period_ms)`; typical 50–100 ms.
- Register sensors at compile time in the manager's static registry.
- Optional: per-sensor priority for update ordering.

## Usage
```cpp
// setup()
SensorManager::init(100);                 // start at 100 ms poll
// loop()
timerSystem.update();                     // service timers only

// consume events
SensorEvent ev;
while (SensorManager::readEvent(ev)) {
  // handle event
}

// consume data
if (SensorManager::isDataReady(sensorId)) {
  MySample s;
  SensorManager::readData(sensorId, &s, sizeof(s));
}
```

## API Reference

### SensorManager
- `init(uint32_t period_ms)`  
  Initializes all registered sensors, starts periodic timer.
- `update()`  
  Called by `TimerSystem`. Iterates sensors, drains events and data.
- `readEvent(SensorEvent& out) -> bool`  
  Pops oldest event from a shared ring buffer. Oldest-drop on overflow.
- `isDataReady(uint8_t sensorId) -> bool`  
- `readData(uint8_t sensorId, void* dst, size_t len)`  
- `health() -> uint32_t` Aggregate status bits across sensors.

### Sensor Driver Contract (uniform)
- `init(const Dependencies&) -> Status`  
- `update()` Nonblocking poll; at most one event per call.  
- `capabilities() -> Caps` bitmask: `DATA`, `EVENT`.  
- `status() -> Status` bitmask: `OK`, `INIT_FAIL`, `COMM_ERR`, `CALIBRATING`, `STALE`.  
- `name() -> const char*`  
- Data path: `isDataReady() -> bool`, `readData(void* dst, size_t len)`  
- Event path: `hasEvent() -> bool`, `readEvent(SensorEvent& out) -> bool`  

### Standard Types
```c
typedef struct {
  uint8_t  type;
  uint8_t  a;
  uint16_t b;
  uint32_t value;
  uint32_t ts_ms;
} SensorEvent;
```

## Events and Data
- Each driver maintains an internal double buffer; `readData()` copies from the inactive buffer for coherence.
- `isDataReady()` is one-shot to prevent duplicate processing.
- Events are exposed by drivers and drained by SensorManager into a shared ring buffer.

## Timing
- One periodic timer created in `init()` calls `update()` at `period_ms`.
- No `delay()`; avoid ad-hoc `millis()` loops inside drivers.
- With signals ≥ 200 ms, a 50–100 ms polling period gives ≤ 100 ms latency.

## Performance
- Target per-sensor `update()` time: ≤ 0.5 ms typical.
- Split long operations across calls using internal FSMs.
- Keep I²C/SPI bursts short to reduce contention with audio/Wi‑Fi/lights.

## Troubleshooting
- `COMM_ERR`: verify pull-ups, bus speed, and exclusive bus use per sensor.
- Starvation: lower polling period or reduce work per `update()`.
- Event loss: increase manager ring size or lower event rate.

## Roadmap
- Optional ISR flags for sub-interval latency sensors
- Dynamic registration / discovery
- Telemetry hook for health and timing stats

## License
MIT



SENSORMANAGER README v1  (plain text friendly)

PURPOSE:
Central dispatcher for multiple sensors with one uniform driver API. No ISRs. Polling via TimerSystem. loop() only services timers.

GOALS:
- Uniform driver API per sensor
- Orchestrator only calls init() once and update() periodically
- Pull-based data/events (no callbacks into app logic)
- Deterministic order, no dynamic allocation
- Nonblocking drivers, short I2C/SPI bursts

ARCHITECTURE:
- Sensor drivers: self-contained; own buffers, FSM, I/O
- SensorManager: thin orchestrator; fixed sensor registry; iterates in priority order
- ContextManager: consumes events and optional data snapshots from SensorManager

TIMING:
- One timer: TimerSystem.setTimer(period, period, 0, SensorManager::update)
- No delay(); no ad-hoc millis() loops in drivers
- Signals >= 200 ms -> typical poll 50–100 ms

STANDARD TYPES:
- SensorEvent { type:u8, a:u8, b:u16, value:u32, ts_ms:u32 }
- Capabilities bitmask: DATA, EVENT
- Status bitmask: OK, INIT_FAIL, COMM_ERR, CALIBRATING, STALE

SENSOR DRIVER API (uniform):
- init(const Dependencies&) -> Status     // bring-up; get bus handles, pins, thresholds
- update()                                 // nonblocking poll; advance FSM; at most 1 event per call
- capabilities() -> Caps
- status() -> Status
- name() -> const char*
- isDataReady() -> bool                    // one-shot
- readData(void* dst, size_t len)          // returns coherent snapshot (driver handles double buffer)
- hasEvent() -> bool
- readEvent(SensorEvent& out) -> bool

SENSORMANAGER API:
- init(uint32_t period_ms)                 // init all sensors in priority order; start periodic timer
- update()                                 // per sensor: update(); drain events; pull ready data; push to shared queues
- readEvent(SensorEvent& out) -> bool      // pop from common ring buffer (oldest-drop on overflow)
- isDataReady(sensorId) -> bool
- readData(sensorId, void* dst, size_t len)
- health() -> aggregate status
- showStatus() optional

REGISTRATION:
- Static registry: Sensor* sensors[] = { &A, &B, &C }
- Optional per-sensor priority for ordering
- One global instance per sensor in its own .cpp

CONCURRENCY/OWNERSHIP:
- I2C/SPI ownership stays inside each sensor; SensorManager only passes refs at init
- No shared mutable state across sensors; communicate via the uniform API only
- No ISRs in v1; if needed later, ISR sets a flag and update() consumes it

DATA CONSISTENCY:
- Each sensor keeps its own double buffer
- readData() copies from inactive buffer
- isDataReady() is one-shot to prevent reprocessing

EVENTS:
- Drivers expose events via hasEvent()/readEvent()
- SensorManager drains into a shared ring buffer
- ContextManager consumes with readEvent(); optional ack flag if a post-read hook is desired

ERRORS/RECOVERY:
- Sticky status() bits per sensor
- Lightweight staged recovery in update() with backoff (no blocking retries)
- SensorManager can emit periodic health events by policy

PERFORMANCE TARGETS:
- Per-sensor update(): <= 0.2–0.5 ms typical
- Split heavy work across calls with internal FSM
- Bound bus usage to avoid contention with audio/Wi-Fi/lights

EXTENSIBILITY:
- Implement the uniform API in a new sensor module
- Add it to the static registry
- No changes required in SensorManager or ContextManager

OUT OF SCOPE (v1):
- Dynamic sensor discovery
- ISR-based paths
- Per-sensor custom event structs
- Cross-sensor fusion inside SensorManager (belongs in ContextManager)
