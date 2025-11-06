#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"

//goal 01:!! sensor-sonar 
//goal 02: calendar
//goal 03:!! lightshows-simple
//goal 04:!!git status un-globalize
//goal 05: OTA
//goal 06: lightsensor
//goal 07: xyz sensor
//goal 08: web interface expand  : silence for X hours, dark for Y hours
//goal 09: lightshows-complex




void setup() {
    Serial.begin(115200);
     while (!Serial) { delay(10); } 
    PL("\n[Main] Version 110608G booting...");// Version MMDDXXA, XX (by Jan indication his list of goals) A (by copilot: A..Z-> update BEFORE any new compilation)
 // version 103002C means : on oct 30, we had (at least) 3 attempts to get the second goal completed

    ConductManager::begin();
   Serial.println("[Main] Setup ready.");
}

void loop() {
    TimerManager::instance().update();
    ConductManager::update();
}