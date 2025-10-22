TimerManager - Cooperative Callback-Based Timer System
Overview
TimerManager is a lightweight, deterministic timer system for embedded platforms like Arduino and ESP32. It avoids interrupts, polling loops, and blocking delays. Timers are identified by their callback functions, and each callback can only be used by one active timer at a time. The system is designed for high concurrency, low jitter, and safe integration with subsystems like FastLED, Wi-Fi, I2S audio, and SD card access.
Features
- Repeating, one-shot, and limited-count timers
- Pause, resume, cancel, and reset by callback identity
- Callback uniqueness enforcement (only one active timer per function)
- Tickless design using millis() for timing
- Safe for high-frequency update() calls (e.g. every 2 ms)
- Minimal memory footprint, no dynamic allocation after setup
- Cooperative multitasking: timers never block or preempt
- Optional minimum interval enforcement (default: 5 ms)
- Debug-only logging for timer creation failures
- Clean function pointer alias: using Callback = void(*)();
Limitations
- No catch-up logic: missed intervals are skipped
- No sub-millisecond resolution (uses millis())
- No dynamic resizing after construction
- No handles or timer IDs—callbacks are the only control mechanism
- Callbacks must be short and non-blocking
Usage
Basic Setup
TimerManager timers(30); // up to 30 concurrent timers
void cb_heartbeat() { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }
void setup() { pinMode(LED_BUILTIN, OUTPUT); timers.create(1000, 0, cb_heartbeat); // blink every 1 second }
void loop() { timers.update(); // call frequently }
Sensor Sampling
void cb_readSensor() { int value = analogRead(A0); Serial.println(value); }
timers.create(200, 0, cb_readSensor); // sample every 200 ms
Button Debounce
void cb_buttonPressed() { Serial.println("Button pressed"); }
timers.create(50, 1, cb_buttonPressed); // debounce delay
Wi-Fi Reconnect
void cb_reconnectWiFi() { WiFi.begin(ssid, password); }
timers.create(5000, 1, cb_reconnectWiFi); // retry after 5 seconds
Internet Fetch
void cb_fetchData() { HTTPClient http; http.begin("http://example.com/data"); int code = http.GET(); Serial.println(code); http.end(); }
timers.create(10000, 0, cb_fetchData); // fetch every 10 seconds
Web Interface Timeout
void cb_sessionExpired() { server.send(403, "text/plain", "Session expired"); }
timers.create(300000, 1, cb_sessionExpired); // 5-minute timeout
FastLED Display Refresh
void cb_refreshDisplay() { FastLED.show(); }
timers.create(50, 0, cb_refreshDisplay); // 20 Hz refresh
Audio Playback from SD via I2S
void cb_audioTick() { // Fill I2S buffer from SD }
timers.create(10, 0, cb_audioTick); // 100 Hz audio tick
Heartbeat LED
void cb_heartbeat() { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }
timers.create(1000, 0, cb_heartbeat); // blink every second
Control Functions
- cancel(callback) — remove timer
- pause(callback) — halt timer, preserving remaining time
- resume(callback) — resume paused timer
- reset(callback) — restart timer from zero
- isActive(callback) — check if timer is running
- isPaused(callback) — check if timer is paused
Best Practices
- Keep callbacks short and non-blocking
- Avoid delay(), long loops, or blocking I/O inside callbacks
- Use millis()-based timing for coarse intervals
- Avoid ultra-fast timers (<5 ms) unless system is idle
- Use oneshot timers for deferred actions (e.g. reconnect, timeout)
- Separate animation logic from display refresh
- Avoid SD or Wi-Fi operations in high-frequency callbacks
Minimum Interval
- Default: 5 ms
- Enforced during create()
- Prevents CPU hogging and subsystem starvation
- Can be adjusted in TimerManager.h via MIN_INTERVAL_MS
Debug Logging
- Enabled via #define DEBUG before including TimerManager
- Logs creation failures to Serial:
- Interval too short
- Callback already in use
- No free slots
Integration Notes
- Works well with FastLED, Wi-Fi, SD, and I2S
- Avoid ultra-fast timers (<5 ms) if Wi-Fi or I2S is active
- Use separate timers for animation logic and display refresh
- Use coarse timers (≥100 ms) for SD or Wi-Fi-related tasks
- Use heartbeat LED to confirm system responsiveness
License
MIT-style. Use freely, modify responsibly.
Author
Doctor Jan — Embedded systems engineer and digital philosopher

Let me know if you'd like this split into sections for documentation, or embedded as comments in your source files.
