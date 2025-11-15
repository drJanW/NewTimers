#include "WiFiManager.h"
#include <WiFi.h>
#include <atomic>
#include "TimerManager.h"
#include "HWconfig.h"
#include "Globals.h"

namespace
{
    TimerManager& timers()
    {
        return TimerManager::instance();
    }

    static constexpr uint32_t POLL_INTERVAL_MS = 250;               // how often we sample WiFi status
    static constexpr uint32_t HEALTH_INTERVAL_MS = 5000;             // watchdog interval after connect
    static constexpr uint32_t INITIAL_RETRY_WINDOW_MS = 1000;        // first connection window
    static constexpr uint32_t RETRY_WINDOW_STEP_MS = 1000;           // growth per failed window
    static constexpr int MAX_RETRIES = 20;

    struct WiFiConnectState {
        bool connected = false;
        bool modeConfigured = false;
        bool pollTimerActive = false;
        bool healthTimerActive = false;
        bool windowTimerActive = false;
        int retryCount = 0;
        uint32_t retryWindowMs = INITIAL_RETRY_WINDOW_MS;

        void resetRetries() {
            retryCount = 0;
            retryWindowMs = INITIAL_RETRY_WINDOW_MS;
        }

        void resetConnectionFlags() {
            connected = false;
        }

        void clearTimers() {
            pollTimerActive = false;
            healthTimerActive = false;
            windowTimerActive = false;
        }
    };

    static WiFiConnectState wifi;

    static constexpr bool WIFI_DEBUG = true;
    static std::atomic<bool> wifiConnected{false};

    void pollTimerHandler();
    void healthTimerHandler();
    void retryWindowHandler();

    bool ensurePollTimer();
    bool ensureHealthTimer();
    bool ensureRetryWindowTimer(uint32_t intervalMs);
    void cancelPollTimer();
    void cancelHealthTimer();
    void cancelRetryWindowTimer();

    void setWiFiConnectedFlag(bool value) {
        wifiConnected.store(value, std::memory_order_relaxed);
    }

    void configureStationMode()
    {
        if (wifi.modeConfigured)
            return;
        WiFi.mode(WIFI_STA);
        wifi.modeConfigured = true;
    }

    void logAttemptWindow()
    {
        if (WIFI_DEBUG)
            PF("[WiFi] Retry %d — window %u ms\n", wifi.retryCount + 1, wifi.retryWindowMs);
    }

    void beginRetryWindow()
    {
        if (wifi.retryCount >= MAX_RETRIES)
        {
            if (WIFI_DEBUG)
                PL("[WiFi] Max retries reached — giving up");
            cancelPollTimer();
            cancelRetryWindowTimer();
            return;
        }

        configureStationMode();
        logAttemptWindow();

        WiFi.disconnect(false);
#if defined(USE_STATIC_IP) && USE_STATIC_IP
        {
            IPAddress local_IP(STATIC_IP_ADDRESS);
            IPAddress gateway(STATIC_GATEWAY);
            IPAddress subnet(STATIC_SUBNET);
            IPAddress dns(STATIC_DNS);
            if (!WiFi.config(local_IP, gateway, subnet, dns) && WIFI_DEBUG)
                PL("[WiFi] Static IP config failed — using DHCP");
        }
#endif
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        ensurePollTimer();
        ensureRetryWindowTimer(wifi.retryWindowMs);
    }

    void startConnectionCampaign(bool resetBackoff)
    {
        cancelHealthTimer();
        cancelRetryWindowTimer();
        wifi.connected = false;
    setWiFiConnectedFlag(false);

        if (resetBackoff)
        {
            wifi.resetRetries();
        }

        beginRetryWindow();
    }

    void onConnected()
    {
        wifi.connected = true;
    setWiFiConnectedFlag(true);
        wifi.resetRetries();

        if (WIFI_DEBUG)
            PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

        cancelRetryWindowTimer();
        cancelPollTimer();
        ensureHealthTimer();
    }

    void onDisconnected()
    {
        if (!wifi.connected)
            return;

        wifi.connected = false;
        setWiFiConnectedFlag(false);
        if (WIFI_DEBUG)
            PL("[WiFi] Lost connection");
    }

    void pollTimerHandler()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
        {
            if (!wifi.connected)
                onConnected();
            return;
        }

        if (wifi.connected)
            onDisconnected();

        // polling continues while attempt timer is active; no extra logic needed
    }

    void retryWindowHandler()
    {
        cancelRetryWindowTimer();

        if (wifi.connected)
        {
            return;
        }

        if (WIFI_DEBUG)
            PF("[WiFi] Retry %d failed (status %d)\n", wifi.retryCount + 1, WiFi.status());

        wifi.retryCount++;
        wifi.retryWindowMs += RETRY_WINDOW_STEP_MS;
        beginRetryWindow();
    }

    void healthTimerHandler()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
            return;

        if (WIFI_DEBUG)
            PL("[WiFi] Health check failed — restarting attempts");
        startConnectionCampaign(true);
    }

    bool ensurePollTimer()
    {
        if (wifi.pollTimerActive)
            return true;
        if (timers().create(POLL_INTERVAL_MS, 0, pollTimerHandler))
        {
            wifi.pollTimerActive = true;
            return true;
        }
        PL("[WiFi] Failed to arm poll timer");
        return false;
    }

    bool ensureHealthTimer()
    {
        if (wifi.healthTimerActive)
            return true;
        if (timers().create(HEALTH_INTERVAL_MS, 0, healthTimerHandler))
        {
            wifi.healthTimerActive = true;
            return true;
        }
        PL("[WiFi] Failed to arm health timer");
        return false;
    }

    bool ensureRetryWindowTimer(uint32_t intervalMs)
    {
        cancelRetryWindowTimer();
        if (timers().create(intervalMs, 1, retryWindowHandler))
        {
            wifi.windowTimerActive = true;
            return true;
        }
        PL("[WiFi] Failed to arm attempt window timer");
        return false;
    }

    void cancelPollTimer()
    {
        timers().cancel(pollTimerHandler);
        wifi.pollTimerActive = false;
    }

    void cancelHealthTimer()
    {
        timers().cancel(healthTimerHandler);
        wifi.healthTimerActive = false;
    }

    void cancelRetryWindowTimer()
    {
        timers().cancel(retryWindowHandler);
        wifi.windowTimerActive = false;
    }
}

void bootWiFiConnect()
{
    startConnectionCampaign(true);
}

bool isWiFiConnected()
{
    return wifiConnected.load(std::memory_order_relaxed);
}
