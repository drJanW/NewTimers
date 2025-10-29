#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"




void setup() {
    Serial.begin(115200);
     while (!Serial) { delay(10); } 
    PL("\n[Main] Version 102801N booting...");// Version MMDDXXA, XX (by Jan indication his list of goals) A (by copilot: A..Z-> update BEFORE any new compilation)
 // version 102801I means : on oct 28, we had (at least) 9 attempts to get 1 goal completed
 
    ConductManager::begin();
    PL("[Main] Setup ready.");
}

void loop() {
    TimerManager::instance().update();
    ConductManager::update();
}