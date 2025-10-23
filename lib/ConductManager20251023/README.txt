===========================================================
SYSTEM ARCHITECTURE OVERVIEW
===========================================================

INPUT SOURCES
-------------
- Web Interface (HTTP routes, sliders, buttons)
- Timers (scheduled events via TimerManager)
- Sensors (lux, temp, voltage, sunrise/sunset)
- OTA triggers (arm/confirm)
- Manual hardware inputs (buttons, GPIO)

FLOW OF CONTROL
---------------
1. Input source generates an INTENT
   (e.g. "play fragment", "say time", "set brightness", "arm OTA")

2. ConductManager receives the intent
   - Central orchestrator
   - Applies context profiles (QuietHours, ChristmasMode, etc.)
   - Delegates to the appropriate Policy

3. Policy layer enforces domain rules
   - AudioPolicy
     * TTS > fragment
     * Reject fragment if busy
     * Cap volume in QuietHours
   - LightPolicy
     * Cap brightness in QuietHours
     * Override palette in ChristmasMode
   - SDPolicy
     * Weighted random fragment selection
     * Score-based arbitration
   - OTAPolicy (tiny)
     * Only allow OTA inside armed window

4. Manager layer executes
   - AudioManager
     * Owns decoder, fades, gain
     * State machine: Idle / PlayingFragment / PlayingTTS / FadingOut
   - LightManager
     * Owns LED state, brightness, patterns
   - SDManager
     * Owns SD index, file access
   - OTAManager
     * Owns OTA arming, confirmation, update streaming
   - WiFiManager
     * Owns connection, retries
   - SensorManager
     * Owns temp, lux, voltage
   - TimerManager
     * Cooperative scheduler for callbacks

5. Hardware drivers
   - I²S audio output
   - FastLED / WS2812B LEDs
   - SD card SPI
   - I²C sensors
   - GPIO pins

DATA FLOW
---------
- SpeakManager: builds phrases (time/date/number) → ConductManager → AudioPolicy → AudioManager
- WebInterfaceManager: parses HTTP → ConductManager intents
- TimerManager: scheduled callbacks → ConductManager intents
- SensorManager: updates values → ConductManager may trigger actions (e.g. lights on at sunset)

STATE MANAGEMENT
----------------
- No global flags in Globals.h
- Each Manager owns its own state (busy flags, levels, values)
- ConductManager queries Managers for state, never pokes globals
- Policies enforce invariants before delegating to Managers

===========================================================
SUMMARY
===========================================================
- ConductManager = WHEN and WHY
- Policies       = WHAT is allowed
- Managers       = HOW it is executed
- Hardware       = Actual effect in the world
===========================================================