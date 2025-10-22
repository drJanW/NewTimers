#include "WiFiManager.h"
#include <WiFi.h>
#include "TimerManager.h"
#include "HWconfig.h"
#include "Globals.h"

namespace
{
    TimerManager& timers()
    {
        return TimerManager::instance();
    }

    static constexpr uint32_t POLL_INTERVAL_MS = 250;
    static constexpr uint32_t HEALTH_INTERVAL_MS = 5000;
    static constexpr int MAX_RETRIES = 20;

    static bool pollTimerArmed = false;
    static bool healthTimerArmed = false;
    static bool modeConfigured = false;

    static bool connected = false;
    static bool attemptActive = false;
    static uint32_t windowMs = 1000;
    static uint16_t pollsRemaining = 0;
    static int attempt = 0;

    static constexpr bool WIFI_DEBUG = true;

    void cb_wifiPoll();   // forward declaration
    void cb_wifiHealth(); // forward declaration

    void disarmTimer(TimerCallback cb, bool& armed)
    {
        if (!armed)
            return;
        timers().cancel(cb);
        armed = false;
    }

    bool armTimer(uint32_t intervalMs, TimerCallback cb, bool& armed)
    {
        if (armed)
            return true;
        if (!timers().create(intervalMs, 0, cb))
        {
            PL("[WiFi] Failed to arm timer");
            return false;
        }
        armed = true;
        return true;
    }

    void configureStationMode()
    {
        if (modeConfigured)
            return;
        WiFi.mode(WIFI_STA);
        modeConfigured = true;
    }

    void logAttemptWindow()
    {
        if (WIFI_DEBUG)
            PF("[WiFi] Attempt %d — window %u ms\n", attempt + 1, windowMs);
    }

    void launchAttempt()
    {
        if (attempt >= MAX_RETRIES)
        {
            if (WIFI_DEBUG)
                PL("[WiFi] Max retries reached — giving up");
            attemptActive = false;
            disarmTimer(cb_wifiPoll, pollTimerArmed);
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

        pollsRemaining = windowMs / POLL_INTERVAL_MS;
        if (pollsRemaining == 0)
            pollsRemaining = 1;
        attemptActive = true;
    }

    void kickoffAttempts(bool resetBackoff)
    {
        disarmTimer(cb_wifiHealth, healthTimerArmed);

        connected = false;
        setWiFiConnected(false);

        if (resetBackoff)
        {
            attempt = 0;
            windowMs = 1000;
        }

        attemptActive = false;
        if (armTimer(POLL_INTERVAL_MS, cb_wifiPoll, pollTimerArmed))
            launchAttempt();
    }

    void onConnected()
    {
        connected = true;
        setWiFiConnected(true);
        attempt = 0;
        windowMs = 1000;
        attemptActive = false;

        if (WIFI_DEBUG)
            PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

        disarmTimer(cb_wifiPoll, pollTimerArmed);
        armTimer(HEALTH_INTERVAL_MS, cb_wifiHealth, healthTimerArmed);
    }

    void onDisconnected()
    {
        if (!connected)
            return;

        connected = false;
        setWiFiConnected(false);
        if (WIFI_DEBUG)
            PL("[WiFi] Lost connection");
    }

    void cb_wifiPoll()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
        {
            if (!connected)
                onConnected();
            return;
        }

        if (connected)
            onDisconnected();

        if (!attemptActive)
        {
            launchAttempt();
            return;
        }

        if (pollsRemaining > 0)
            pollsRemaining--;

        if (pollsRemaining == 0)
        {
            if (WIFI_DEBUG)
                PF("[WiFi] Attempt %d failed (status %d)\n", attempt + 1, WiFi.status());
            attempt++;
            windowMs += 1000;
            attemptActive = false;
            launchAttempt();
        }
    }

    void cb_wifiHealth()
    {
        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE)
            return;

        if (WIFI_DEBUG)
            PL("[WiFi] Health check failed — restarting attempts");
        kickoffAttempts(true);
    }
}

void bootWiFiConnect()
{
    kickoffAttempts(true);
}
