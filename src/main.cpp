#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"




void setup() {
    Serial.begin(115200);
     while (!Serial) { delay(10); } 
    PL("\n[Main] System starting...");
    ConductManager::begin();
    PL("[Main] Setup ready.");
}

void loop() {
    TimerManager::instance().update();
    ConductManager::update();
}