// =============================================
// FetchManager.cpp  (clean TimerManager version)
// =============================================

#include "FetchManager.h"
#include "Globals.h"
#include "TimerManager.h"
#include "WiFiManager.h"
#include "PRTClock.h"
#include "ContextManager.h"
#include "SDManager.h"

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Timezone.h>
#include <HTTPClient.h>

static void cacheTimeSnapshotTick();
static void persistClockSnapshot();

namespace {
    TimerManager &TM() { return TimerManager::instance(); }
    PRTClock &clockSvc() { return PRTClock::instance(); }
}

namespace {
    constexpr const char *LAST_TIME_CACHE_PATH = "/last_sync.txt";
    constexpr uint32_t TIME_CACHE_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour

    static bool timeCacheTimerScheduled = false;

    static bool ensureTimeCacheTimer() {
        if (timeCacheTimerScheduled) {
            return true;
        }
        if (TM().create(TIME_CACHE_INTERVAL_MS, 0, cacheTimeSnapshotTick)) {
            timeCacheTimerScheduled = true;
            PF("[Fetch] Time cache timer armed (%lu ms)\n", (unsigned long)TIME_CACHE_INTERVAL_MS);
            return true;
        }
        PL("[Fetch] Failed to schedule time cache timer");
        return false;
    }
}

// --- Config ---
#define SUN_URL     "https://api.sunrise-sunset.org/json?lat=52.3702&lng=4.8952&formatted=0"
#define WEATHER_URL "https://api.open-meteo.com/v1/forecast?latitude=52.37&longitude=4.89&daily=temperature_2m_max,temperature_2m_min&timezone=auto"

#define DEBUG_FETCH 1

// --- Timezone for Europe/Amsterdam ---
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET  = {"CET", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);

// --- NTP Client ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// --- Forward declarations ---
static void cb_tryNTP();
static void cb_tryWeather();
static void cb_trySun();
static bool fetchUrlToString(const char *url, String &output);
static void persistClockSnapshotFromTm(const struct tm &t);
static bool loadPersistedClockSnapshot(PRTClock &clock);



// ===================================================
// NTP / Time fetch
// ===================================================
static void cb_tryNTP() {
    static bool clientStarted = false;
    static bool wifiWarned = false;

    if (isTimeFetched()) {
        return;
    }

    if (!isWiFiConnected()) {
        if (DEBUG_FETCH && !wifiWarned) {
            PL("[Fetch] No WiFi, waiting before NTP");
            wifiWarned = true;
        }
        return;
    }

    if (!clientStarted) {
        timeClient.begin();
        clientStarted = true;
    }

    if (DEBUG_FETCH) {
        PL("[Fetch] Trying NTP/time fetch...");
    }

    if (!timeClient.update()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] NTP update failed, will retry");
        }
        return;
    }

    wifiWarned = false;

    time_t utc   = timeClient.getEpochTime();
    time_t local = CE.toLocal(utc);

    struct tm t;
    localtime_r(&local, &t);

    auto &clk = clockSvc();
    clk.setHour(t.tm_hour);
    clk.setMinute(t.tm_min);
    clk.setSecond(t.tm_sec);
    clk.setYear(t.tm_year + 1900 - 2000);
    clk.setMonth(t.tm_mon + 1);
    clk.setDay(t.tm_mday);
    clk.setDoW(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    clk.setDoY(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

    if (DEBUG_FETCH) {
        PF("[Fetch] Time update: %02d:%02d:%02d (%d-%02d-%02d)\n",
           t.tm_hour, t.tm_min, t.tm_sec,
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }

    persistClockSnapshotFromTm(t);
    ensureTimeCacheTimer();

    setTimeFetched(true);
    clk.setMoonPhaseValue();
}

// ===================================================
// Weather fetch
// ===================================================

static void cb_tryWeather() {
    if (!isWiFiConnected()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping weather");
        }
        ContextManager::clearWeather();
        return;
    }
    if (!isTimeFetched()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No NTP/time, skipping weather");
        }
        ContextManager::clearWeather();
        return;
    }

    String response;
    if (!fetchUrlToString(WEATHER_URL, response)) {
        if (DEBUG_FETCH) {
            PL("[Fetch] Weather fetch failed, will retry");
        }
        ContextManager::clearWeather();
        return;
    }

    int idxMin = response.indexOf("\"temperature_2m_min\":[");
    int idxMax = response.indexOf("\"temperature_2m_max\":[");
    if (idxMin == -1 || idxMax == -1) {
        ContextManager::clearWeather();
        return;
    }

    int minStart = response.indexOf("[", idxMin);
    int minEnd   = response.indexOf("]", idxMin);
    int maxStart = response.indexOf("[", idxMax);
    int maxEnd   = response.indexOf("]", idxMax);
    if (minStart == -1 || minEnd == -1 || maxStart == -1 || maxEnd == -1) {
        ContextManager::clearWeather();
        return;
    }

    float tMin = response.substring(minStart + 1, minEnd).toFloat();
    float tMax = response.substring(maxStart + 1, maxEnd).toFloat();
    ContextManager::updateWeather(tMin, tMax);

    if (DEBUG_FETCH) {
        PF("[Fetch] Ext. Temperature: min=%.1f max=%.1f\n", tMin, tMax);
    }

    TM().cancel(cb_tryWeather);
}

// ===================================================
// Sunrise/Sunset fetch
// ===================================================

static void cb_trySun() {
    if (!isWiFiConnected()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No WiFi, skipping sun data");
        }
        return;
    }
    if (!isTimeFetched()) {
        if (DEBUG_FETCH) {
            PL("[Fetch] No NTP/time, skipping sun data");
        }
        return;
    }

    String response;
    if (!fetchUrlToString(SUN_URL, response)) {
        if (DEBUG_FETCH) {
            PL("[Fetch] Sun fetch failed, will retry");
        }
        return;
    }

    int sr = response.indexOf("\"sunrise\":\"");
    int ss = response.indexOf("\"sunset\":\"");
    if (sr == -1 || ss == -1) return;

    String sunriseIso = response.substring(sr + 11, sr + 30);
    String sunsetIso  = response.substring(ss + 10, ss + 29);

    struct tm tmRise = {};
    struct tm tmSet  = {};

    tmRise.tm_year = sunriseIso.substring(0, 4).toInt() - 1900;
    tmRise.tm_mon  = sunriseIso.substring(5, 7).toInt() - 1;
    tmRise.tm_mday = sunriseIso.substring(8, 10).toInt();
    tmRise.tm_hour = sunriseIso.substring(11, 13).toInt();
    tmRise.tm_min  = sunriseIso.substring(14, 16).toInt();
    tmRise.tm_sec  = sunriseIso.substring(17, 19).toInt();

    tmSet.tm_year = sunsetIso.substring(0, 4).toInt() - 1900;
    tmSet.tm_mon  = sunsetIso.substring(5, 7).toInt() - 1;
    tmSet.tm_mday = sunsetIso.substring(8, 10).toInt();
    tmSet.tm_hour = sunsetIso.substring(11, 13).toInt();
    tmSet.tm_min  = sunsetIso.substring(14, 16).toInt();
    tmSet.tm_sec  = sunsetIso.substring(17, 19).toInt();

    time_t utcRise = mktime(&tmRise);
    time_t utcSet  = mktime(&tmSet);

    time_t localRise = CE.toLocal(utcRise);
    time_t localSet  = CE.toLocal(utcSet);

    struct tm ltmRise, ltmSet;
    localtime_r(&localRise, &ltmRise);
    localtime_r(&localSet, &ltmSet);

    auto &clk = clockSvc();
    clk.setSunriseHour(ltmRise.tm_hour);
    clk.setSunriseMinute(ltmRise.tm_min);
    clk.setSunsetHour(ltmSet.tm_hour);
    clk.setSunsetMinute(ltmSet.tm_min);

    if (DEBUG_FETCH) {
        PF("[Fetch] Sunrise/Sunset (local): up %02d:%02d, down %02d:%02d\n",
           ltmRise.tm_hour, ltmRise.tm_min, ltmSet.tm_hour, ltmSet.tm_min);
    }

    TM().cancel(cb_trySun);
}

// ===================================================
// Init entry point
// ===================================================
bool bootFetchManager() {
    if (!isWiFiConnected()) {
        if (DEBUG_FETCH) PL("[Fetch] WiFi not ready, fetchers not scheduled");
        return false;
    }

    TM().cancel(cb_tryNTP);
    TM().cancel(cb_tryWeather);
    TM().cancel(cb_trySun);
    TM().cancel(cacheTimeSnapshotTick);
    timeCacheTimerScheduled = false;

    setTimeFetched(false);
    bool ntpOk = TM().create(1000, 0, cb_tryNTP);
    bool weatherOk = TM().create(2000, 0, cb_tryWeather);
    bool sunOk = TM().create(3000, 0, cb_trySun);

    if (!ntpOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule NTP timer");
    if (!weatherOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule weather timer");
    if (!sunOk && DEBUG_FETCH) PL("[Fetch] Failed to schedule sun timer");

    return ntpOk && weatherOk && sunOk;
}

void updateFetchManager() {
    // Timer-driven implementation does not require a poll loop.
}

static void persistClockSnapshot() {
    if (!isSDReady()) {
        return;
    }

    auto &clk = clockSvc();
    uint16_t year = 2000u + clk.getYear();
    uint8_t month = clk.getMonth();
    uint8_t day = clk.getDay();
    uint8_t hour = clk.getHour();
    uint8_t minute = clk.getMinute();
    uint8_t second = clk.getSecond();

    char payload[48];
    int written = snprintf(payload, sizeof(payload), "%04u-%02u-%02uT%02u:%02u:%02u\n",
                           (unsigned)year, (unsigned)month, (unsigned)day,
                           (unsigned)hour, (unsigned)minute, (unsigned)second);
    if (written <= 0 || written >= static_cast<int>(sizeof(payload))) {
        return;
    }

    if (!SDManager::instance().writeTextFile(LAST_TIME_CACHE_PATH, payload)) {
        PL("[Fetch] Failed to persist cached time snapshot");
    }
}

static void cacheTimeSnapshotTick() {
    if (!isTimeFetched()) {
        return;
    }
    persistClockSnapshot();
}

static void persistClockSnapshotFromTm(const struct tm &t) {
    if (!isSDReady()) {
        return;
    }

    char payload[48];
    int written = snprintf(payload, sizeof(payload), "%04d-%02d-%02dT%02d:%02d:%02d\n",
                           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                           t.tm_hour, t.tm_min, t.tm_sec);
    if (written <= 0 || written >= static_cast<int>(sizeof(payload))) {
        return;
    }

    if (!SDManager::instance().writeTextFile(LAST_TIME_CACHE_PATH, payload)) {
        PL("[Fetch] Failed to persist cached time snapshot");
    }
}

static bool loadPersistedClockSnapshot(PRTClock &clock) {
    if (!isSDReady()) {
        return false;
    }

    auto &sd = SDManager::instance();
    if (!sd.fileExists(LAST_TIME_CACHE_PATH)) {
        return false;
    }

    String content = sd.readTextFile(LAST_TIME_CACHE_PATH);
    content.trim();
    if (content.length() < 19) {
        return false;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf(content.c_str(), "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return false;
    }

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return false;
    }

    clock.setYear(static_cast<uint8_t>(year - 2000));
    clock.setMonth(static_cast<uint8_t>(month));
    clock.setDay(static_cast<uint8_t>(day));
    clock.setHour(static_cast<uint8_t>(hour));
    clock.setMinute(static_cast<uint8_t>(minute));
    clock.setSecond(static_cast<uint8_t>(second));
    clock.setDoW(static_cast<uint8_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day));
    clock.setDoY(static_cast<uint8_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day));
    clock.setMoonPhaseValue();

    PF("[Fetch] Seeded clock from cached snapshot: %s\n", content.c_str());
    return true;
}

bool fetchLoadCachedTime(PRTClock &clock) {
    return loadPersistedClockSnapshot(clock);
}
// ===================================================
// Low-level HTTP fetch
// ===================================================
static bool fetchUrlToString(const char *url, String &output) {
    if (!isWiFiConnected()) return false;

    HTTPClient http;
    int httpCode = 0;
    for (uint8_t iTry = 1; iTry <= 3; iTry++) {
        http.begin(url);
        httpCode = http.GET();
        if (httpCode == 200) break;
        http.end();
        delay(5);
    }
    if (httpCode != 200) {
        if (DEBUG_FETCH) {
            PF("[Fetch] HTTP GET failed: %s\n", url);
        }
        return false;
    }
    output = http.getString();
    http.end();
    return (output.length() > 0);
}
