#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"

//goal 01:sensor-sonar
//goal 02: calendar
//goal 03: lightshows-simple
//goal 04: OTA
//goal 05: lightshows-complex



void setup() {
    Serial.begin(115200);
     while (!Serial) { delay(10); } 
    PL("\n[Main] Version 103002C booting...");// Version MMDDXXA, XX (by Jan indication his list of goals) A (by copilot: A..Z-> update BEFORE any new compilation)
 // version 103002C means : on oct 30, we had (at least) 3 attempts to get a second goal completed

    ConductManager::begin();
   Serial.println("[Main] Setup ready.");
}

void loop() {
    TimerManager::instance().update();
    ConductManager::update();
}