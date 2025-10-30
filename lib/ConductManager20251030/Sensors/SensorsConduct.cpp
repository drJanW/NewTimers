#include "SensorsConduct.h"

#include "Globals.h"
#include "SensorManager.h"
#include "SensorsPolicy.h"
#include "TimerManager.h"

#include "Heartbeat/HeartbeatConduct.h"
#include "Heartbeat/HeartbeatPolicy.h"
#include "../Audio/AudioConduct.h"
#include "../Audio/AudioPolicy.h"
#include "../Light/LightConduct.h"

namespace {

constexpr uint8_t SENSOR_EVENT_DISTANCE = 0x30;
constexpr uint32_t SENSOR_INTERVAL_MS = 100;

bool s_distancePlaybackEligible = false;

void processSensorEvents() {
    SensorEvent ev;

    while (SensorManager::readEvent(ev)) {
        if (ev.type != SENSOR_EVENT_DISTANCE) {
            continue;
        }

        float distanceMm = static_cast<float>(ev.value);
        float filteredMm = 0.0f;
        if (!SensorsPolicy::normaliseDistance(distanceMm, ev.ts_ms, filteredMm)) {
            continue;
        }

        distanceMm = filteredMm;
        if (distanceMm <= 0.0f) {
            continue;
        }

        uint32_t interval = 0;
        if (HeartbeatPolicy::intervalFromDistance(distanceMm, interval)) {
            heartbeatConduct.setRate(interval);
        }

        uint32_t audioIntervalMs = 0;
        const bool audioEligible = AudioPolicy::distancePlaybackInterval(distanceMm, audioIntervalMs);
    (void)audioIntervalMs;

        if (audioEligible) {
            if (!s_distancePlaybackEligible) {
                s_distancePlaybackEligible = true;
                AudioConduct::startDistanceResponse(true);
            }
        } else if (s_distancePlaybackEligible) {
            s_distancePlaybackEligible = false;
            TimerManager::instance().cancel(AudioConduct::cb_playPCM);
        }

        LightConduct::handleDistanceReading(distanceMm);
    }
}

} // namespace

void SensorsConduct::plan() {
    auto &tm = TimerManager::instance();
    s_distancePlaybackEligible = false;
    if (!tm.restart(SENSOR_INTERVAL_MS, 0, processSensorEvents)) {
        PF("[SensorsConduct] Failed to schedule sensor processing timer\n");
    } else {
        PF("[Conduct][Plan] Sensor processing timer scheduled (%lu ms)\n",
           static_cast<unsigned long>(SENSOR_INTERVAL_MS));
    }
}
