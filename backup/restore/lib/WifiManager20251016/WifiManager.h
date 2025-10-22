#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void startWiFiConnect();     // Begin non-blocking connection
bool isWiFiConnected();      // Query connection status

#endif