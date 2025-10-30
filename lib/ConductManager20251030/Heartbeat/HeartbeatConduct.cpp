#include "HeartbeatConduct.h"

#include "Globals.h"
#include "TimerManager.h"
#include "HeartbeatPolicy.h"
#include "ConductManager.h"

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

uint32_t heartbeatIntervalMs = 0;
bool ledState = false;

void heartbeatTick() {
    if (ConductManager::isQuietHoursActive()) {
        ledState = false;
    } else {
        ledState = !ledState;
    }

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    setLedStatus(ledState);
}

} // namespace

HeartbeatConduct heartbeatConduct;

void HeartbeatConduct::plan() {
	HeartbeatPolicy::configure();
	const uint32_t defaultInterval = HeartbeatPolicy::defaultIntervalMs();

    TimerManager &tm = TimerManager::instance();
    if (!tm.restart(defaultInterval, 0, heartbeatTick)) {
		PF("[HeartbeatConduct] Failed to schedule heartbeat timer\n");
	} else {
        heartbeatIntervalMs = defaultInterval;
		HB_LOG("[HeartbeatConduct] Timer started at %lums\n", static_cast<unsigned long>(heartbeatIntervalMs));
	}
}

void HeartbeatConduct::setRate(uint32_t intervalMs) {
	intervalMs = HeartbeatPolicy::clampInterval(intervalMs);
    if (intervalMs == heartbeatIntervalMs) {
		return;
	}

	TimerManager &tm = TimerManager::instance();
    const bool speedingUp = (heartbeatIntervalMs == 0) ? false : (intervalMs < heartbeatIntervalMs);
    if (!tm.restart(intervalMs, 0, heartbeatTick)) {
        PF("[HeartbeatConduct] Failed to adjust heartbeat interval to %lu ms\n",
           static_cast<unsigned long>(intervalMs));
        return;
    } else {
        heartbeatIntervalMs = intervalMs;
        HB_LOG("[HeartbeatConduct] Interval set to %lums\n",
               static_cast<unsigned long>(heartbeatIntervalMs));
        if (speedingUp) {
            heartbeatTick(); // reflect the higher tempo immediately on the LED
        }
    }
}

uint32_t HeartbeatConduct::currentRate() const {
    return heartbeatIntervalMs;
}

void HeartbeatConduct::trigger() {
	heartbeatTick();
}
