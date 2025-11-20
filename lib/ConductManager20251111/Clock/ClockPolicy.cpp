#include "ClockPolicy.h"

#include "Globals.h"
#include "PRTClock.h"

#include <Wire.h>
#include <RTClib.h>
#include <math.h>

namespace {
    RTC_DS3231 g_rtc;
    bool g_i2cStarted = false;
    bool g_probed = false;
    bool g_rtcReady = false;
    bool g_powerLost = false;
    float g_lastTemperatureC = NAN;

    void ensureI2C() {
        if (g_i2cStarted) {
            return;
        }
        Wire.begin(I2C_SDA, I2C_SCL);
        g_i2cStarted = true;
    }

    bool ensureRTC() {
        if (g_rtcReady) {
            return true;
        }
        if (!g_probed) {
            g_probed = true;
            ensureI2C();
            g_rtcReady = g_rtc.begin();
            if (!g_rtcReady) {
                PL("[RTC] DS3231 not detected on I2C bus");
                return false;
            }
        } else {
            return false;
        }
        g_powerLost = g_rtc.lostPower();
        if (g_powerLost) {
            PL("[RTC] Power lost; set time manually");
        }
        g_lastTemperatureC = g_rtc.getTemperature();
        return true;
    }

    DateTime buildDateTimeFromClock(const PRTClock &clock) {
        uint16_t year = static_cast<uint16_t>(2000U + clock.getYear());
        uint8_t month = clock.getMonth();
        uint8_t day = clock.getDay();
        uint8_t hour = clock.getHour();
        uint8_t minute = clock.getMinute();
        uint8_t second = clock.getSecond();
        return DateTime(year, month ? month : 1, day ? day : 1,
                        hour, minute, second);
    }
}

namespace ClockPolicy {

void configureHardware() {
    (void)ensureRTC();
}

bool isRtcAvailable() {
    return ensureRTC();
}

bool seedClockFromRTC(PRTClock &clock) {
    if (!ensureRTC()) {
        return false;
    }
    DateTime now = g_rtc.now();
    if (now.year() < 2000 || now.year() > 2099) {
        return false;
    }
    clock.setYear(static_cast<uint8_t>(now.year() - 2000));
    clock.setMonth(static_cast<uint8_t>(now.month()));
    clock.setDay(static_cast<uint8_t>(now.day()));
    clock.setHour(static_cast<uint8_t>(now.hour()));
    clock.setMinute(static_cast<uint8_t>(now.minute()));
    clock.setSecond(static_cast<uint8_t>(now.second()));
    clock.setDoW(static_cast<uint8_t>(now.year()), static_cast<uint8_t>(now.month()), static_cast<uint8_t>(now.day()));
    clock.setDoY(static_cast<uint8_t>(now.year()), static_cast<uint8_t>(now.month()), static_cast<uint8_t>(now.day()));
    clock.setMoonPhaseValue();
    g_lastTemperatureC = g_rtc.getTemperature();
    if (!isnan(g_lastTemperatureC)) {
        PF("[RTC] Seeded clock from RTC read (%04d-%02d-%02d %02d:%02d:%02d) [%.2f C]\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           static_cast<double>(g_lastTemperatureC));
    } else {
        PF("[RTC] Seeded clock from RTC read (%04d-%02d-%02d %02d:%02d:%02d)\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
    }
    return true;
}

void syncRTCFromClock(const PRTClock &clock) {
    if (!ensureRTC()) {
        return;
    }
    DateTime dt = buildDateTimeFromClock(clock);
    if (dt.year() < 2000 || dt.year() > 2099) {
        return;
    }
    g_rtc.adjust(dt);
    g_powerLost = false;
    g_lastTemperatureC = g_rtc.getTemperature();
    PF("[RTC] Synced hardware clock to %04d-%02d-%02d %02d:%02d:%02d\n",
       dt.year(), dt.month(), dt.day(),
       dt.hour(), dt.minute(), dt.second());
}

float lastTemperatureC() {
    return g_lastTemperatureC;
}

bool wasPowerLost() {
    return g_powerLost;
}

} // namespace ClockPolicy
