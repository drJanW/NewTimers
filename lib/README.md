An Art Project - Jellyfish
Overzicht van het project:
Hoofdfunctionaliteiten:
•	Audio Management - MP3 afspelen met random fragmenten en TTS (VoiceRSS)
•	LED Management - 160 WS2812B LEDs met complexe lichtshows
•	Web Interface - Bediening via browser (volume, licht, voting systeem)
•	WiFi & OTA - Netwerkconnectie en wireless updates
•	Tijd & Sensoren - NTP tijd, zonsop/ondergang, sensoren
•	SD Kaart - Audio files en indexsysteem
Enkele interessante aspecten:
Audio Systeem
•	Gebruikt ESP8266Audio library met I2S output
•	Ondersteunt zowel lokale MP3 files als TTS via VoiceRSS API
•	"Voting systeem" voor audiofragmenten (likes/dislikes)
Licht Shows
•	Complexe algoritmes voor cirkel-patronen, gradienten, en animaties
•	Positie-gebaseerde LED mapping (x,y coördinaten van SD kaart)
•	Dynamische brightness gebaseerd op audio levels en web input
Web Interface
•	Moderne HTML/CSS/JS interface
•	Real-time sliders voor volume en brightness
•	Voting buttons voor audio content

Context Manager becomes the runtime brain: gathers state from the environment, normalizes it, and surfaces “what’s happening now” to the rest of the system; timer-driven updates and sensor snapshots all feed into that shared context.
Conduct Manager is the decision layer: it consumes context plus intent inputs (user/web/etc.), applies high-level rules, and emits intents toward subsystems (audio, light, OTA) without touching hardware details itself.
Each Policy (AudioPolicy, LightPolicy, SDPolicy, etc.) is now a focused ruleset: given a request and the current context, it enforces local constraints (quiet hours, brightness caps, playback arbitration) before delegating to the subsystem managers.
Net effect: context collects facts, conduct chooses actions, policies enforce domain-specific guardrails—clean separation that keeps subsystems modular and easier to evolve.

A TimerManager slot fires and runs its callback (e.g., hourly “say time”, periodic fragment shuffle).
Each callback raises an intent toward ConductManager (or directly queues a ContextManager refresh), never touching hardware.
ConductManager combines the intent with the current context snapshot, then consults the relevant policy (audio/light/SD/etc.).
The policy enforces its domain rules—quiet hours, resource availability, safety thresholds—and either rejects or forwards the request.
Approved requests go to the subsystem manager (AudioManager, LightManager, …), which executes the action; rejections are logged or deferred.

 producers → Context → Conduct → policies → intents → consumers (audio/light/serial)

 layered manager-based architecture:

Modules: encapsulate specific functionality (audio, light, input, etc.).

ContextManager: decides current state/environment (time, rules, events).

ConductManager: applies behavior patterns given the context.

PolicyManager: enforces priorities, resolves conflicts between conducts.

TimerManager: centralized timing, fade scheduling, FSM ticks.

Each manager has one global instance, and responsibilities are strictly separated. Dependencies flow top-down: context → conduct → policy → modules, with TimerManager as shared service