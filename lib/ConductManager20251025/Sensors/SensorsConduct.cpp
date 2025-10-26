#include "SensorsConduct.h"

#include "Globals.h"
#include "SensorManager.h"
#include "AudioManager.h"
#include "TimerManager.h"
#include <math.h>

#include "Heartbeat/HeartbeatConduct.h"
#include "Heartbeat/HeartbeatPolicy.h"

namespace {

constexpr uint8_t SENSOR_EVENT_DISTANCE = 0x30;
void processSensorEvents() {
    SensorEvent ev;
    bool intervalUpdated = false;

    while (SensorManager::readEvent(ev)) {
        if (ev.type != SENSOR_EVENT_DISTANCE) {
            continue;
        }

        float distanceMm = static_cast<float>(ev.value);
        if (distanceMm <= 0.0f) {
            continue;
        }

        uint32_t interval = 0;
        if (HeartbeatPolicy::intervalFromDistance(distanceMm, interval)) {
            heartbeatConduct.setRate(interval);
            intervalUpdated = true;
        }

    AudioManager::instance().notifySonarDistance(distanceMm);
    }

    if (!intervalUpdated) {
        float bootstrap = getSensorValue();
        uint32_t interval = 0;
        if (HeartbeatPolicy::bootstrap(bootstrap, interval)) {
            heartbeatConduct.setRate(interval);
        }
    }
}

} // namespace

void SensorsConduct::plan() {
    auto &tm = TimerManager::instance();
    tm.cancel(processSensorEvents);
    if (!tm.create(100, 0, processSensorEvents)) {
        PF("[SensorsConduct] Failed to schedule sensor processing timer\n");
    } else {
        PF("[SensorsConduct] Sensor processing timer scheduled\n");
    }
}
