#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"
#include "OTAManager.h"
#include "Globals.h"

// goal 00: cleanup and refactor existing code
// goal 01:!! sensor-sonar
// goal 02: calendar & day-of-time & seasons
// goal 03:!! lightshows-simple
// goal 04:!! git status un-globalize
// goal 05:!! OTA
// goal 06: lightsensor
// goal 07: xyz sensor
// goal 08:!! web interface TODO: expand  : silence for X hours, dark for Y hours
// goal 09:!! lightshows-complex
// goal 10:!! RTC on i2c
// goal 11: watchdog
// goal 12: error handling/alarm
// goal 13: context/conduct manager 


void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(10);
    }
    PL("\n[Main] Version 11_25_02_E"); // Version MM_DD_XX_A, XX (by Jan indication his list of goals) A (by copilot: A..Z-> update BEFORE any new compilation)
                                    // version 10_30_02_C means : on oct 30, we had (at least) 3 attempts to get the second goal completed

    otaBootDispatcher();
    ConductManager::begin();
    Serial.println("[Main] Setup ready.");
}

void loop()
{
    TimerManager::instance().update();
    ConductManager::update();
}
