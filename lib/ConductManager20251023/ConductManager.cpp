
#include "ConductManager.h"

#include "ConductBoot.h"
#include "TimerManager.h"
#include "Globals.h"
#include "ContextManager.h"
#include "PRTClock.h"
#include "BootMaster.h"
#include "AudioManager.h"

#include "Status/StatusBoot.h"
#include "Status/StatusConduct.h"
#include "Status/StatusPolicy.h"

#include "WiFi/WiFiBoot.h"
#include "WiFi/WiFiConduct.h"
#include "WiFi/WiFiPolicy.h"

#include "Web/WebBoot.h"
#include "Web/WebConduct.h"
#include "Web/WebPolicy.h"

#include "Audio/AudioBoot.h"
#include "Audio/AudioConduct.h"
#include "Audio/AudioPolicy.h"

#include "Light/LightBoot.h"
#include "Light/LightConduct.h"
#include "Light/LightPolicy.h"

#include "Sensors/SensorsBoot.h"
#include "Sensors/SensorsConduct.h"
#include "Sensors/SensorsPolicy.h"

#include "OTA/OTABoot.h"
#include "OTA/OTAConduct.h"
#include "OTA/OTAPolicy.h"

#include "Speak/SpeakBoot.h"
#include "Speak/SpeakConduct.h"

#include "SD/SDBoot.h"
#include "SD/SDConduct.h"
#include "SD/SDPolicy.h"

#include "FetchManager.h"
#include "WiFiManager.h"

namespace {

TimerManager &timers() {
    return TimerManager::instance();
}

String twoDigits(uint8_t v) {
    if (v < 10) {
        String result("0");
        result += String(v);
        return result;
    }
    return String(v);
}

constexpr uint32_t TIMER_STATUS_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
constexpr uint32_t TIME_DISPLAY_INTERVAL_MS = 10 * 12000;    // ~1.7 minutes
constexpr uint32_t HEARTBEAT_MIN_MS = 200;
constexpr uint32_t HEARTBEAT_DEFAULT_MS = 500;
constexpr uint32_t HEARTBEAT_MAX_MS = 2000;

void clockTick() {
    PRTClock::instance().update();
}

String buildTimePhrase() {
    const auto &timeCtx = ContextManager::time();
    String phrase("Het is ");
    phrase += twoDigits(timeCtx.hour);
    phrase += ':';
    phrase += twoDigits(timeCtx.minute);
    return phrase;
}

String buildNowPhrase() {
    const auto &timeCtx = ContextManager::time();
    String phrase("Het is nu ");
    phrase += weekdayName(timeCtx.dayOfWeek);
    phrase += ' ';
    phrase += String(timeCtx.day);
    phrase += ' ';
    phrase += monthName(timeCtx.month);
    phrase += " om ";
    phrase += twoDigits(timeCtx.hour);
    phrase += ':';
    phrase += twoDigits(timeCtx.minute);
    return phrase;
}

} // namespace

bool ConductManager::christmasMode = false;
bool ConductManager::quietHours = false;

static TimerCallback heartbeatCb = nullptr;
static uint32_t heartbeatInterval = HEARTBEAT_DEFAULT_MS;
static TimerCallback clockCb = clockTick;
static bool clockRunning = false;
static bool clockInFallback = false;

static ConductBoot bootPlanner;
static StatusConduct statusConduct;
static SDBoot sdBoot;
static SDConduct sdConduct;
static WiFiBoot wifiBoot;
static WiFiConduct wifiConduct;
static WebBoot webBoot;
static WebConduct webConduct;
static AudioBoot audioBoot;
static AudioConduct audioConduct;
static LightBoot lightBoot;
static LightConduct lightConduct;
static SensorsBoot sensorsBoot;
static SensorsConduct sensorsConduct;
static OTABoot otaBoot;
static OTAConduct otaConduct;
static SpeakBoot speakBoot;
static SpeakConduct speakConduct;

void ConductManager::begin() {
    bool sayTimeScheduled = timers().create(3600000, 0, [](){ intentSayTime(); });
    bool fragmentScheduled = timers().create(600000, 0, [](){ intentPlayFragment(); });
    bool initialFragmentScheduled = timers().create(5000, 1, [](){ intentPlayFragment(); });
    bool timerStatusScheduled = timers().create(TIMER_STATUS_INTERVAL_MS, 0, [](){ intentShowTimerStatus(); });
    bool timeDisplayScheduled = timers().create(TIME_DISPLAY_INTERVAL_MS, 0, timeDisplayTick);
    bool bootMasterScheduled = bootMaster.begin();
    heartbeatCb = [](){ intentPulseStatusLed(); };
    bool heartbeatScheduled = timers().create(heartbeatInterval, 0, heartbeatCb);
    PF("[Conduct] begin(): sayTime=%d fragment=%d initialFrag=%d timerStatus=%d timeDisplay=%d bootMaster=%d\n",
       sayTimeScheduled, fragmentScheduled, initialFragmentScheduled, timerStatusScheduled, timeDisplayScheduled, bootMasterScheduled);
    if (!bootMasterScheduled) {
        PL("[Conduct] clock bootstrap timer failed");
    }
    if (!heartbeatScheduled) {
        PL("[Conduct] heartbeat timer failed");
    }
    if (!initialFragmentScheduled) {
        PL("[Conduct] initial fragment timer failed");
    }

    bootPlanner.plan();
    statusBoot.plan();
    statusConduct.plan();
    sdBoot.plan();
    sdConduct.plan();
    wifiBoot.plan();
    wifiConduct.plan();
    webBoot.plan();
    webConduct.plan();
    audioBoot.plan();
    audioConduct.plan();
    lightBoot.plan();
    lightConduct.plan();
    sensorsBoot.plan();
    sensorsConduct.plan();
    otaBoot.plan();
    otaConduct.plan();
    speakBoot.plan();
    speakConduct.plan();
}

void ConductManager::update() {
    AudioManager::instance().update();
    applyContextOverrides();
}

void ConductManager::intentPlayFragment() {
    AudioFragment frag;
    if (!SDPolicy::getRandomFragment(frag)) {
        PF("[Conduct] intentPlayFragment: no fragment available\n");
        return;
    }
    if (!AudioPolicy::canPlayFragment()) {
        PF("[Conduct] intentPlayFragment: blocked by policy\n");
        return;
    }

    PF("[Conduct] intentPlayFragment: dir=%u file=%u\n", frag.dirIndex, frag.fileIndex);
    if (!AudioManager::instance().startFragment(frag)) {
        PF("[Conduct] intentPlayFragment: failed to start fragment\n");
    }
}

void ConductManager::intentSayTime() {
    String phrase = buildTimePhrase();
    if (!AudioPolicy::canPlaySentence()) {
        PF("[Conduct] intentSayTime blocked by policy\n");
        return;
    }

    PF("[Conduct] intentSayTime: %s\n", phrase.c_str());
    AudioManager::instance().startTTS(phrase);
}

void ConductManager::intentSayNow() {
    String phrase = buildNowPhrase();
    if (christmasMode) phrase = "Vrolijk Kerstfeest! " + phrase;
    if (!AudioPolicy::canPlaySentence()) {
        PF("[Conduct] intentSayNow blocked by policy\n");
        return;
    }

    PF("[Conduct] intentSayNow: %s\n", phrase.c_str());
    AudioManager::instance().startTTS(phrase);
}

void ConductManager::intentSetBrightness(float value) {
    float adjusted = LightPolicy::applyBrightnessRules(value, quietHours);
    LightManager::instance().setBrightness(adjusted);
}

void ConductManager::intentSetAudioLevel(float value) {
    float adjusted = AudioPolicy::applyVolumeRules(value, quietHours);
    AudioManager::instance().setWebLevel(adjusted);
}

void ConductManager::intentArmOTA(uint32_t window_s) {
    PF("[Conduct] intentArmOTA: window=%us\n", (unsigned)window_s);
    otaArm(window_s);
    AudioManager::instance().stop();
    LightManager::instance().showOtaPattern();
}

void ConductManager::intentConfirmOTA() {
    PF("[Conduct] intentConfirmOTA\n");
    otaConfirmAndReboot();
}

void ConductManager::intentShowTimerStatus() {
    TimerManager::instance().showAvailableTimers(true);
}

void ConductManager::intentPulseStatusLed() {
    static bool ledState = false;

    if (quietHours) {
        ledState = false;
    } else {
        ledState = !ledState;
    }

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    setLedStatus(ledState);
}

void ConductManager::intentSetHeartbeatRate(uint32_t intervalMs) {
    if (intervalMs < HEARTBEAT_MIN_MS) intervalMs = HEARTBEAT_MIN_MS;
    if (intervalMs > HEARTBEAT_MAX_MS) intervalMs = HEARTBEAT_MAX_MS;

    if (heartbeatInterval == intervalMs) return;

    heartbeatInterval = intervalMs;
    auto &tm = TimerManager::instance();
    tm.cancel(heartbeatCb);
    if (!tm.create(heartbeatInterval, 0, heartbeatCb)) {
        PF("[Conduct] Failed to adjust heartbeat interval to %lu ms\n", (unsigned long)heartbeatInterval);
    } else {
        PF("[Conduct] Heartbeat interval set to %lu ms\n", (unsigned long)heartbeatInterval);
    }
}

void ConductManager::setChristmasMode(bool enabled) {
    christmasMode = enabled;
}

void ConductManager::setQuietHours(bool enabled) {
    quietHours = enabled;
}

void ConductManager::applyContextOverrides() {
    static bool quietLogged = false;
    if (quietHours) {
        LightManager::instance().capBrightness(50);
        AudioManager::instance().capVolume(0.3f);
        if (!quietLogged) {
            PF("[Conduct] applyContextOverrides: quiet hours active\n");
            quietLogged = true;
        }
    } else if (quietLogged) {
        PF("[Conduct] applyContextOverrides: quiet hours cleared\n");
        quietLogged = false;
    }
}

bool ConductManager::intentStartClockTick(bool fallbackMode) {
    auto &tm = TimerManager::instance();

    if (clockRunning && clockInFallback == fallbackMode) {
        return true;
    }

    if (clockRunning) {
        tm.cancel(clockCb);
        clockRunning = false;
    }

    if (!tm.create(SECONDS_TICK, 0, clockCb)) {
        PF("[Conduct] Failed to start clock tick (%s)\n", fallbackMode ? "fallback" : "normal");
        return false;
    }

    clockRunning = true;
    clockInFallback = fallbackMode;
    PF("[Conduct] Clock tick running (%s)\n", fallbackMode ? "fallback" : "normal");
    return true;
}

bool ConductManager::isClockRunning() {
    return clockRunning;
}

bool ConductManager::isClockInFallback() {
    return clockInFallback;
}