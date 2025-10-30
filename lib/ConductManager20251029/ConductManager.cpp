
#include "ConductManager.h"

#include "ConductBoot.h"
#include "TimerManager.h"
#include "Globals.h"
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

#include "Heartbeat/HeartbeatBoot.h"
#include "Heartbeat/HeartbeatConduct.h"
#include "Heartbeat/HeartbeatPolicy.h"

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

#include "Calendar/CalendarBoot.h"
#include "Calendar/CalendarConduct.h"

#include "FetchManager.h"
#include "WiFiManager.h"

#ifndef LOG_CONDUCT_VERBOSE
#define LOG_CONDUCT_VERBOSE 0
#endif

#if LOG_CONDUCT_VERBOSE
#define CONDUCT_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define CONDUCT_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define CONDUCT_LOG_INFO(...)
#define CONDUCT_LOG_DEBUG(...)
#endif

#define CONDUCT_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define CONDUCT_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

namespace {

TimerManager &timers() {
    return TimerManager::instance();
}

constexpr uint32_t TIMER_STATUS_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
constexpr uint32_t TIME_DISPLAY_INTERVAL_MS = 10 * 12000;    // ~1.7 minutes

void clockTick() {
    PRTClock::instance().update();
}

} // namespace

bool ConductManager::christmasMode = false;
bool ConductManager::quietHours = false;

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
static bool sdPostBootCompleted = false;

void ConductManager::begin() {
    bool sayTimeScheduled = timers().create(3600000, 0, [](){ intentSayTime(); });
    bool fragmentScheduled = timers().create(600000, 0, [](){ intentPlayFragment(); });
    bool initialFragmentScheduled = timers().create(5000, 1, [](){ intentPlayFragment(); });
    bool timerStatusScheduled = timers().create(TIMER_STATUS_INTERVAL_MS, 0, [](){ intentShowTimerStatus(); });
    bool timeDisplayScheduled = timers().create(TIME_DISPLAY_INTERVAL_MS, 0, timeDisplayTick);
    bool bootMasterScheduled = bootMaster.begin();
    CONDUCT_LOG_INFO("[Conduct] begin(): sayTime=%d fragment=%d initialFrag=%d timerStatus=%d timeDisplay=%d bootMaster=%d\n",
                     sayTimeScheduled, fragmentScheduled, initialFragmentScheduled, timerStatusScheduled, timeDisplayScheduled, bootMasterScheduled);
    if (!bootMasterScheduled) {
        CONDUCT_LOG_WARN("[Conduct] clock bootstrap timer failed\n");
    }
    if (!initialFragmentScheduled) {
        CONDUCT_LOG_WARN("[Conduct] initial fragment timer failed\n");
    }

    bootPlanner.plan();
    heartbeatBoot.plan();
    heartbeatConduct.plan();
    statusBoot.plan();
    statusConduct.plan();

    if (!sdBoot.plan()) {
        return;
    }

    resumeAfterSDBoot();
}

void ConductManager::update() {
    AudioManager::instance().update();
    applyContextOverrides();
#if LOG_HEARTBEAT
    static uint32_t lastHeartbeatMs = 0;
    uint32_t now = millis();
    if (now - lastHeartbeatMs >= 1000U) {
        LOG_HEARTBEAT_TICK('.');
        lastHeartbeatMs = now;
    }
#endif
}

void ConductManager::intentPlayFragment() {
    AudioFragment frag;
    if (!SDPolicy::getRandomFragment(frag)) {
        CONDUCT_LOG_INFO("[Conduct] intentPlayFragment: no fragment available\n");
        return;
    }
    if (!AudioPolicy::requestFragment(frag)) {
        CONDUCT_LOG_INFO("[Conduct] intentPlayFragment: request rejected\n");
        return;
    }
    CONDUCT_LOG_DEBUG("[Conduct] intentPlayFragment: dir=%u file=%u\n", frag.dirIndex, frag.fileIndex);
}

void ConductManager::intentSayTime() {
    String phrase = PRTClock::instance().buildTimeSentence();
    if (!AudioPolicy::canPlaySentence()) {
        CONDUCT_LOG_INFO("[Conduct] intentSayTime blocked by policy\n");
        return;
    }

    CONDUCT_LOG_DEBUG("[Conduct] intentSayTime: %s\n", phrase.c_str());
    AudioManager::instance().startTTS(phrase);
}

void ConductManager::intentSayNow() {
    String phrase = PRTClock::instance().buildNowSentence();
    if (christmasMode) phrase = "Vrolijk Kerstfeest! " + phrase;
    if (!AudioPolicy::canPlaySentence()) {
        CONDUCT_LOG_INFO("[Conduct] intentSayNow blocked by policy\n");
        return;
    }

    CONDUCT_LOG_DEBUG("[Conduct] intentSayNow: %s\n", phrase.c_str());
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
    CONDUCT_LOG_INFO("[Conduct] intentArmOTA: window=%us\n", (unsigned)window_s);
    otaArm(window_s);
    AudioManager::instance().stop();
    LightManager::instance().showOtaPattern();
}

bool ConductManager::intentConfirmOTA() {
    CONDUCT_LOG_INFO("[Conduct] intentConfirmOTA\n");
    return otaConfirmAndReboot();
}

void ConductManager::intentShowTimerStatus() {
    TimerManager::instance().showAvailableTimers(true);
}

void ConductManager::setChristmasMode(bool enabled) {
    christmasMode = enabled;
}

void ConductManager::setQuietHours(bool enabled) {
    quietHours = enabled;
}

bool ConductManager::isQuietHoursActive() {
    return quietHours;
}

void ConductManager::applyContextOverrides() {
    static bool quietLogged = false;
    if (quietHours) {
        LightManager::instance().capBrightness(50);
        AudioManager::instance().capVolume(0.3f);
        if (!quietLogged) {
            CONDUCT_LOG_INFO("[Conduct] applyContextOverrides: quiet hours active\n");
            quietLogged = true;
        }
    } else if (quietLogged) {
        CONDUCT_LOG_INFO("[Conduct] applyContextOverrides: quiet hours cleared\n");
        quietLogged = false;
    }
}

bool ConductManager::intentStartClockTick(bool fallbackMode) {
    auto &tm = TimerManager::instance();

    if (clockRunning && clockInFallback == fallbackMode) {
        return true;
    }

    bool wasRunning = clockRunning;
    if (!tm.restart(SECONDS_TICK, 0, clockCb)) {
        CONDUCT_LOG_ERROR("[Conduct] Failed to start clock tick (%s)\n", fallbackMode ? "fallback" : "normal");
        if (wasRunning) {
            clockRunning = false;
        }
        return false;
    }

    clockRunning = true;
    clockInFallback = fallbackMode;
    CONDUCT_LOG_INFO("[Conduct] Clock tick running (%s)\n", fallbackMode ? "fallback" : "normal");
    return true;
}

bool ConductManager::isClockRunning() {
    return clockRunning;
}

bool ConductManager::isClockInFallback() {
    return clockInFallback;
}

void ConductManager::intentNextLightShow() {
    CONDUCT_LOG_DEBUG("[Conduct] intentNextLightShow\n");
    nextImmediate();
    intentPlayFragment();
}

void ConductManager::resumeAfterSDBoot() {
    if (sdPostBootCompleted) {
        return;
    }

    sdPostBootCompleted = true;

    sdConduct.plan();
    calendarBoot.plan();
    calendarConduct.plan();
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