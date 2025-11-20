
#include "WiFiBoot.h"
#include "Globals.h"
#include "WiFiManager.h"
#include "WiFiPolicy.h"
#include "FetchManager.h"
#include "TimerManager.h"
#include "ConductManager.h"
#include <WiFi.h>

namespace {
    static bool fetchScheduled = false;
    static bool subsystemTimerStarted = false;
    static bool subsystemsReadyAnnounced = false;

    void subsystemInitTick() {
        if (!ConductManager::isClockRunning()) {
            return;
        }

        if (!subsystemsReadyAnnounced) {
            subsystemsReadyAnnounced = true;
            if (ConductManager::isClockInFallback()) {
                PL("[Main] Bootstrapping (fallback time) ready");
            } else {
                PL("[Main] Bootstrapping (normal time) ready");
            }
        }

        auto &timers = TimerManager::instance();
        timers.cancel(subsystemInitTick);
        subsystemTimerStarted = false;
    }

    constexpr uint32_t WIFI_WAIT_LOG_INTERVAL_MS = 5000;

    void wifiBootPollTick() {
        static bool lastWiFiState = false;
        static uint32_t lastWaitLogMs = 0;

        bool wifiUp = isWiFiConnected();
        if (wifiUp && !lastWiFiState) {
            PF("[Main] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
        } else if (!wifiUp && lastWiFiState) {
            PL("[Main] WiFi lost, retrying");
        }

        if (!wifiUp) {
            const uint32_t now = millis();
            if (now - lastWaitLogMs >= WIFI_WAIT_LOG_INTERVAL_MS) {
                PL("[Main] WiFi not connected yet");
                lastWaitLogMs = now;
            }
        }
        else {
            lastWaitLogMs = millis();
        }

        lastWiFiState = wifiUp;

        if (wifiUp) {
            auto &timers = TimerManager::instance();

            if (!fetchScheduled) {
                timers.cancel(wifiBootPollTick);
                if (bootFetchManager()) {
                    fetchScheduled = true;
                    PL("[Main] Fetch schedulers running");
                } else {
                    PL("[Main] Fetch schedulers failed to start");
                }
            }

            if (!subsystemTimerStarted) {
                if (timers.create(1000, 0, subsystemInitTick)) {
                    subsystemTimerStarted = true;
                    PL("[Main] Subsystem monitor timer started");
                } else {
                    PL("[Main] Failed to start subsystem timer");
                }
            }
        }
    }
}

void WiFiBoot::plan() {
    auto &timers = TimerManager::instance();
    if (!timers.create(1000, 0, wifiBootPollTick)) {
        PL("[Main] Failed to create WiFi boot poll timer");
    }
    bootWiFiConnect();
    PL("[Conduct][Plan] WiFi connect sequence started");
    WiFiPolicy::configure();
}
