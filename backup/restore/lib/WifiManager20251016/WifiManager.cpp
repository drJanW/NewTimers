#include "WiFiManager.h"
#include <WiFi.h>
#include "TimerManager.h"
#include "HWconfig.h"
#include "Globals.h"

extern TimerManager timers;

namespace {
  static bool connected = false;
  static int attempt = 0;
  static int delayMs = 1000;
  static const int maxRetries = 20;
  static int remainingPolls = 0;

  static constexpr bool WIFI_DEBUG = true;

  void cb_wifiPoll();   // forward declaration
  void cb_wifiHealth(); // forward declaration

  void beginAttempt() {
    if (WIFI_DEBUG) PF("[WiFi] Attempt %d — window %d ms\n", attempt + 1, delayMs);

    WiFi.disconnect(false);
#if defined(USE_STATIC_IP) && USE_STATIC_IP
    {
      IPAddress local_IP(STATIC_IP_ADDRESS);
      IPAddress gateway(STATIC_GATEWAY);
      IPAddress subnet(STATIC_SUBNET);
      IPAddress dns(STATIC_DNS);
      if (!WiFi.config(local_IP, gateway, subnet, dns)) {
        if (WIFI_DEBUG) PL("[WiFi] Static IP config failed — using DHCP");
      }
    }
#endif
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // how many polls in this attempt window
    remainingPolls = delayMs / 250;
    if (remainingPolls < 1) remainingPolls = 1;

    // schedule the first poll
    timers.create(250, 1, cb_wifiPoll);
  }

  void cb_wifiPoll() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
      if (!connected) {
        connected = true;
        if (WIFI_DEBUG) PF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        timers.create(5000, 0, cb_wifiHealth); // periodic health check
      }
      return;
    }

    if (connected) {
      connected = false;
      if (WIFI_DEBUG) PL("[WiFi] Lost connection");
    }

    if (--remainingPolls > 0) {
      // reschedule next poll
      timers.create(250, 1, cb_wifiPoll);
      return;
    }

    // attempt window expired
    attempt++;
    if (attempt >= maxRetries) {
      if (WIFI_DEBUG) PL("[WiFi] Max retries reached — giving up");
      return;
    }

    delayMs += 1000;
    beginAttempt();  // start next attempt
  }

  void cb_wifiHealth() {
    if (WiFi.status() != WL_CONNECTED) {
      if (WIFI_DEBUG) PL("[WiFi] Lost connection — restarting attempts");
      connected = false;
      attempt = 0;
      delayMs = 1000;
      beginAttempt();
    }
  }
}

void startWiFiConnect() {
  attempt = 0;
  delayMs = 1000;
  connected = false;
  WiFi.mode(WIFI_STA);
  beginAttempt();
}

bool isWiFiConnected() { return connected; }
