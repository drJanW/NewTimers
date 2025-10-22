
#include "ConductManager.h"
void ConductManager::update() {
    // TODO: Implement update logic if needed
}

#include "ConductBoot.h"
#include "TimerManager.h"
#include "Globals.h"
#include "System/SystemBoot.h"
#include "BootMaster.h"
#include "Status/StatusBoot.h"
#include "ContextManager.h"
#include "PRTClock.h"

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
#include "Speak/SpeakPolicy.h"

#include "SD/SDBoot.h"
#include "SD/SDConduct.h"
#include "SD/SDPolicy.h"

#include "fetchManager.h"
#include "WifiManager.h"

namespace {

TimerManager &timers() {
    return TimerManager::instance();
}

constexpr uint32_t TIMER_STATUS_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
constexpr uint32_t TIME_DISPLAY_INTERVAL_MS = 10 * 12000;    // ~1.7 minutes
constexpr uint32_t CLOCK_BOOTSTRAP_INTERVAL_MS = 1000;
constexpr uint32_t NTP_FALLBACK_TIMEOUT_MS = 60 * 1000;
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


// BootMaster now in BootMaster.h/cpp
static ConductBoot bootPlanner;
// StatusBoot statusBoot; // Now defined in StatusBoot.cpp
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

void ConductManager::begin() {
    // Boot all subsystems first
    TimerManager::instance().showAvailableTimers(true);
}

void ConductManager::intentPulseStatusLed() {
    static bool ledState = false;

    // Simple gate: reuse quiet-hours mute if needed
    if (quietHours) {
        ledState = false;
    } else {
        ledState = !ledState;
    }

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    setLedStatus(ledState);
}

void ConductManager::intentSetHeartbeatRate(uint32_t intervalMs) {
    // Forward to SystemBoot or appropriate module
    // (No-op: heartbeat timer logic is now modular)
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
    // Forward to SystemBoot or appropriate module
    // (No-op: clock tick timer logic is now modular)
    return false;
}

bool ConductManager::isClockRunning() {
    // Forward to SystemBoot or appropriate module
    return false;
}

bool ConductManager::isClockInFallback() {
    // Forward to SystemBoot or appropriate module
    return false;
}