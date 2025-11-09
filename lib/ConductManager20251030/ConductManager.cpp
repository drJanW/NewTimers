
#include "ConductManager.h"

#include <vector>

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
#include "Web/WebDirector.h"
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

#include "Calendar.h"//<==================
#include "SDManager.h"//<=================

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

constexpr const char* kLightShowsCsvPath = "/light_shows.csv";

std::vector<String> s_lightShowIds;
size_t s_lightShowCursor = 0;
bool s_lightShowListLoaded = false;

bool shouldSkipPresetLine(const String& line) {
    if (line.isEmpty()) {
        return true;
    }
    if (line.startsWith("#") || line.startsWith("//")) {
        return true;
    }
    return false;
}

size_t splitSemicolon(const String& line, String* columns, size_t maxColumns) {
    if (!columns || maxColumns == 0) {
        return 0;
    }
    size_t count = 0;
    int start = 0;
    const int length = line.length();
    while (count < maxColumns && start <= length) {
        int sep = line.indexOf(';', start);
        if (sep < 0) {
            sep = length;
        }
        String token = line.substring(start, sep);
        token.trim();
        columns[count++] = token;
        start = sep + 1;
        if (sep >= length) {
            break;
        }
    }
    return count;
}

bool loadLightShowPresets() {
    s_lightShowIds.clear();
    s_lightShowCursor = 0;

    if (!SDManager::isReady()) {
        CONDUCT_LOG_WARN("[Conduct] Light preset load skipped, SD not ready\n");
        return false;
    }

    String payload = SDManager::instance().readTextFile(kLightShowsCsvPath);
    if (payload.length() == 0) {
        CONDUCT_LOG_WARN("[Conduct] Light preset list empty (%s)\n", kLightShowsCsvPath);
        return false;
    }

    int start = 0;
    const int len = payload.length();
    while (start < len) {
        int end = payload.indexOf('\n', start);
        String line = (end >= 0) ? payload.substring(start, end) : payload.substring(start);
        start = (end >= 0) ? end + 1 : len;
        line.trim();
        if (shouldSkipPresetLine(line)) {
            continue;
        }

        String columns[4];
        const size_t count = splitSemicolon(line, columns, 4);
        if (count < 2) {
            continue;
        }
        if (columns[0].equalsIgnoreCase("light_show_id") || columns[0].isEmpty()) {
            continue;
        }
        s_lightShowIds.push_back(columns[0]);
    }

    if (s_lightShowIds.empty()) {
        CONDUCT_LOG_WARN("[Conduct] No light presets found in %s\n", kLightShowsCsvPath);
        return false;
    }

    CONDUCT_LOG_INFO("[Conduct] Loaded %u light presets from %s\n",
                     static_cast<unsigned>(s_lightShowIds.size()),
                     kLightShowsCsvPath);
    return true;
}

bool ensureLightShowPresets() {
    if (s_lightShowListLoaded && !s_lightShowIds.empty()) {
        return true;
    }
    s_lightShowListLoaded = loadLightShowPresets();
    return s_lightShowListLoaded;
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
    if (!calendarManager.isReady()) {
        CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: calendar manager not ready\n");
        return;
    }

    if (!ensureLightShowPresets()) {
        CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: no light presets available\n");
        return;
    }

    const size_t total = s_lightShowIds.size();
    if (total == 0) {
        CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: light preset list empty\n");
        return;
    }

    const size_t startIndex = s_lightShowCursor % total;
    for (size_t offset = 0; offset < total; ++offset) {
        const size_t index = (startIndex + offset) % total;
        const String& presetId = s_lightShowIds[index];

        CalendarLightShow show;
        if (!calendarManager.lookupLightShow(presetId, show)) {
            CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: failed to load preset %s\n",
                             presetId.c_str());
            continue;
        }
        if (!show.valid || !show.pattern.valid) {
            CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: preset %s invalid\n",
                             presetId.c_str());
            continue;
        }

        CalendarColorRange none{};
        LightPolicy::applyCalendarLightshow(show, none);
        s_lightShowCursor = (index + 1) % total;
        CONDUCT_LOG_INFO("[Conduct] intentNextLightShow: applied preset %s\n", presetId.c_str());
        return;
    }

    CONDUCT_LOG_WARN("[Conduct] intentNextLightShow: no playable presets found\n");
    LightPolicy::clearCalendarLightshow();
    s_lightShowListLoaded = false;
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
    WebDirector::instance().plan();
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