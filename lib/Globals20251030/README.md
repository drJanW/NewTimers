now: 20251029

contains "all" parameters used by more than one module or file

## Atomics helper pattern

`Globals.cpp` exposes helper templates for light-weight atomic storage. Typical usage:

```cpp
template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    ptr->store(value, std::memory_order_relaxed);
}
template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    return ptr->load(std::memory_order_relaxed);
}

static std::atomic<uint8_t> _valYear{0};
void setYear(uint8_t v) { setMux(v, &_valYear); }
uint8_t getYear() { return getMux(&_valYear); }
```

Most global state accessors follow this pattern and should migrate into dedicated managers over time.

`HWconfig.h` keeps hardware settings (pins, Wi-Fi credentials, static IP, etc.). `adc_port` is still only used to seed randomness and should be refactored out later.

## Logging controls (`macros.inc`)

`macros.inc` centralizes logging macros for every module:

- Default build uses `LOG_LEVEL_INFO`. Override per build via PlatformIO flag, e.g. `-DLOG_LEVEL=LOG_LEVEL_WARN` for quieter output or `LOG_LEVEL_DEBUG` for full tracing.
- Heartbeat dots are guarded by `LOG_HEARTBEAT`; enable with `-DLOG_HEARTBEAT=1` when you want a periodic serial heartbeat, otherwise leave at `0` to avoid clutter.
- Module-level verbosity toggles such as `LOG_CONDUCT_VERBOSE`, `LOG_TIMER_VERBOSE`, `LOG_AUDIO_VERBOSE`, etc. gate the chattier traces. Define them as `1` (either in `platformio.ini` or a local header) when diagnosing that subsystem.
- Use `PL()/PP()/PF()` helpers for INFO-level lines; `LOG_ERROR/LOG_WARN/LOG_INFO/LOG_DEBUG` remain available for formatted output.

When the serial monitor shows nothing except ESP-IDF boot ROM text, confirm the firmware was built with at least `LOG_LEVEL_INFO` so the `[Main] Version …` banner is visible.
