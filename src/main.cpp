#include <Arduino.h>

#include "TimerManager.h"
#include "ConductManager.h"
#include "OTAManager.h"
#include "Globals.h"
#include <Wire.h>
#include <RTClib.h>

// goal 01:!! sensor-sonar
// goal 02: calendar
// goal 03:!! lightshows-simple
// goal 04:!! git status un-globalize
// goal 05:!! OTA
// goal 06: lightsensor
// goal 07: xyz sensor
// goal 08:!! web interface TODO: expand  : silence for X hours, dark for Y hours
// goal 09:!! lightshows-complex
// goal 10: RTC on i2c
// goal 11: error handling/alarm

namespace {
RTC_DS3231 g_rtc;

void probeRTC()
{
    if (!g_rtc.begin())
    {
        Serial.println("[RTC] DS3231 not detected on I2C bus");
        return;
    }

    if (g_rtc.lostPower())
    {
        Serial.println("[RTC] Power lost; set time manually");
    }

    const DateTime now = g_rtc.now();
    Serial.printf("[RTC] Now %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());

    Serial.printf("[RTC] Temperature %.2f C\n", g_rtc.getTemperature());
}
} // namespace

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(10);
    }
    PL("\n[Main] Version 11_15_02_F"); // Version MM_DD_XX_A, XX (by Jan indication his list of goals) A (by copilot: A..Z-> update BEFORE any new compilation)
                                    // version 10_30_02_C means : on oct 30, we had (at least) 3 attempts to get the second goal completed

    //Wire.begin(I2C_SDA, I2C_SCL);
   // probeRTC();

    otaBootDispatcher();
    ConductManager::begin();
    Serial.println("[Main] Setup ready.");
}

void loop()
{
    TimerManager::instance().update();
    ConductManager::update();
}
