#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>


void bootWiFiConnect();     // Begin non-blocking connection
void planWiFiBoot();         // Orchestrate WiFi/fetch boot logic
bool isWiFiConnected();

#endif