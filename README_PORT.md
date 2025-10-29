# PORTERINGSHANDLEIDING — Terug naar stabiel + verbeteringen

Deze gids is gemaakt om vanaf **Backup_2025-10-11_1602.zip** veilig te porten naar de stabiele situatie met de recente verbeteringen. Volg exact de volgorde. Geen gokwerk.

## 0) Hygiëne
- **Eén** actieve map per subsystem (AudioManager, LightManager, WebInterfaceManager, TimerSystem, FetchManager).
- Zet oudere versiemappen uit met `.pioignore` of hernoem ze (bijv. `lib/_old.AudioManager20250920/`).
- Verwijder `.pio/` bij grote wijzigingen. Build schoon.

---

## 1) TimerSystem — `kill()` en `INVALID_TIMER_ID`
**Bestanden:** `lib/TimerSystem*/TimerSystem.h`, `lib/TimerSystem*/TimerSystem.cpp`

**Doelen:**
- Introduceer `INVALID_TIMER_ID = 255` (constexpr).
- Vervang alle `id != 255`-patronen.
- Gebruik **één** semantiek voor stoppen+invalideren.

**API (samenvatting):**
```cpp
// TimerSystem.h
static constexpr uint8_t INVALID_TIMER_ID = 255;

class TimerSystem {
public:
  void    kill(uint8_t &id);   // stopt timer en zet id op INVALID_TIMER_ID
  uint8_t kill(uint8_t id);    // stopt timer en retourneert INVALID_TIMER_ID
};
```

**Gebruik overal (voorbeeld):**
```cpp
// Vroeger:
/* if (ntpRetryTimerId != 255) { timerSystem.finishTimer(ntpRetryTimerId); ntpRetryTimerId = 255; } */

// Nu:
ntpRetryTimerId = timerSystem.kill(ntpRetryTimerId);
// of by-ref:
timerSystem.kill(ntpRetryTimerId);
```

---

## 2) FetchManager — NTP-retry correct stoppen
**Bestand:** `lib/WiFiManager*/fetchManager.cpp`

**Beleid:**
- Eén retry-timer (`ntpRetryTimerId`).
- Bij **succes**: `timerSystem.kill(ntpRetryTimerId)` en log “Time update. No more retries.”
- Na succes **geen** nieuwe fetches meer.

**Skelet (indicatief):**
```cpp
static uint8_t ntpRetryTimerId = INVALID_TIMER_ID;

static void cb_tryFetchTime() {
  if (doFetchTime()) {
    PF("[FetchManager] Time update. No more retries.\n");
    timerSystem.kill(ntpRetryTimerId);
  } else {
    PF("[FetchManager] Now trying NTP/time fetch...\n");
  }
}

void initFetchManager() {
  if (ntpRetryTimerId == INVALID_TIMER_ID) {
    ntpRetryTimerId = timerSystem.setTimer(0, 10000, 0, cb_tryFetchTime); // bv. elke 10 s
  }
}
```

---

## 3) WebInterfaceManager — routes “next” + slider-debounce
**Bestand:** `lib/WebInterfaceManager*/WebInterfaceManager.cpp`  
**HTML:** behoud je bestaande `index.html`.

**Routes toevoegen:**
```cpp
server.on("/light/next", HTTP_GET, [](AsyncWebServerRequest* req){
  LightManager::nextImmediate();
  req->send(200, "text/plain", "OK");
});

server.on("/audio/next", HTTP_GET, [](AsyncWebServerRequest* req){
  audioManager.nextImmediate();
  req->send(200, "text/plain", "OK");
});
```

**Slider-debounce (2 s “definitief”):**
- Houd per slider een timer-id: `audioLevelDebounceId`, `brightnessDebounceId`.
- Bij wijziging: cache waarde, `timerSystem.kill(id)`, start one-shot 2 s die pas dan toepast en één regel logt.

Pseudocode:
```cpp
// bij POST/GET van slider
cachedAudioLevel = nieuw;
timerSystem.kill(audioLevelDebounceId);
audioLevelDebounceId = timerSystem.setTimer(2000, 0, 0, cb_applyAudioLevel);

static void cb_applyAudioLevel() {
  applyAudioLevel(cachedAudioLevel);
  PF("[Web] AudioLevel definitief: %.2f\n", cachedAudioLevel);
}
```

---

## 4) LightManager — `nextImmediate()`
**Bestand:** `lib/LightManager*/LightManager.cpp`

**Semantiek:**
- Stop huidige show, kill interne show-timers (`timerSystem.kill(showTimerIdX)`).
- Start **altijd** een volgende show, ook als er geen loopt.

Checklist:
1) `stopCurrentShow()`
2) `kill()` alle show-timers
3) `currentShow = pickNext()`
4) `startShow(currentShow)`

---

## 5) AudioManager — `nextImmediate()` en service-loop
**Bestand:** `lib/AudioManager*/AudioManager.cpp`

**Regels:**
- `stopImmediate()` routeert naar `PlayFragment::stopImmediate()`.
- Eén globale instantie, één set callbacks.
- **Geen** lokale `isAudioBusy()` hier; gebruik de globale uit `Globals`.
- Service-loop: als `reqRandomFrag && !isAudioBusy()`, dan `getRandomFragment()` + `start()`.
- Eenmalige trigger bij boot is genoeg (bv. `reqRandomFrag = true;` aan eind van `begin()`). Periodiek is optioneel.

**Niet aanpassen in deze ronde:** `PlayFragment.*` uit 1602 ongemoeid laten.

---

## 6) Geen globale “wait for network” bij boot
- Verwijder blokkerende boot-wachters.
- Gebruik **contextuele** check vlak vóór netwerkactie: `if (!WiFiConnected()) return;`

Resultaat: snelle boot. WiFi/NTP mogen later aansluiten zonder alles te blokkeren.

---

## 7) Logging
- Centrale macros staan in `lib/Globals20251027/macros.inc` (`LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `LOG_DEBUG`).
- Standaard build gebruikt `LOG_LEVEL_ERROR`; zet `-DLOG_LEVEL=LOG_LEVEL_DEBUG` in `platformio.ini` voor volledige trace.
- Domeinspecifieke switches (`LOG_AUDIO_VERBOSE`, `LOG_PCM_VERBOSE`, `LOG_CONDUCT_VERBOSE`, `LOG_TIMER_VERBOSE`) houden drukke subsystems stil zolang ze op `0` blijven.
- Enable `LOG_HEARTBEAT=1` voor een één-karakter heartbeat (punt) via `ConductManager::update()` zonder extra spam.
- Direct gebruik van `Serial.printf/println` vermijden; kies het juiste macro-niveau en voeg alleen contextrijke foutmeldingen toe.

---

## 8) Verificatie
- Buildlog moet **slechts één** `PlayFragment.cpp` en **één** `AudioManager.cpp` tonen.
- Geen `undefined reference` of “redefinition” meldingen.
- Serieel bij cold boot:
  - “Init klaar.”
  - 0–2 NTP-pogingen → “Time update. No more retries.”
  - Binnen enkele seconden: hoorbaar fragment, lichtshow #0 start.

---

## 9) Wat NIET porten nu
- Geen nieuwe OTA-hooks (`beginOTA`, etc.).
- Geen extra TTS-webroutes.
- Geen alternatieve `isAudioBusy()` definities buiten `Globals`.
- Geen stubs in `PlayFragment.*`.

---

## 10) Veelgemaakte valkuilen
- Meerdere versie-mappen actief ⇒ dubbele definities of stubs winnen.
- `.pio/` niet gewist na grote refactors ⇒ oude objectbestanden linken mee.
- `kill()` niet gebruikt ⇒ timers blijven hangen en veroorzaken ruis of overlap.

---

## Korte checklist per stap
1) Eén actieve map per module ✔
2) `TimerSystem.kill()` + `INVALID_TIMER_ID` ✔
3) FetchManager: één retry-timer, kill bij succes ✔
4) Web: `/light/next` + `/audio/next` + slider-debounce ✔
5) LightManager: `nextImmediate()` stopt en start schoon ✔
6) AudioManager: geen lokale `isAudioBusy()`, boot-trigger ✔
7) Geen boot-wachters, alleen contextuele checks ✔
8) Logging sane ✔
9) Clean build, rooktest ✔

---

Alleen jouw TimerSystem API gebruiken. Geen extra libs, geen delay().

Timer-lifecycle via kill() en INVALID_TIMER_ID. Geen id != 255.

Geen nieuwe varianten zoals finishAndInvalidate. Geen “Flush”.

Eén callback per timer. Geen verborgen of periodieke timers zonder jouw toestemming.

Bij succes (bv. NTP) de retry-timer direct killen en ID ongeldig maken.

Geen globale waitForNetwork(). Alleen contextuele checks vlak vóór netwerk-calls.

Geen OTA/beginOTA of andere features die jij niet vroeg.

Werken altijd vanaf jouw meest recente project-zip; difffen, dan complete bestanden teruggeven.
