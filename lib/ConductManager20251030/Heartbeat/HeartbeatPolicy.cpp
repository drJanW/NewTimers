#include "HeartbeatPolicy.h"

#include "Globals.h"
#include <math.h>

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

constexpr uint32_t HEARTBEAT_MIN_MS = 90;       // very close -> rapid flash interval
constexpr uint32_t HEARTBEAT_MAX_MS = 2000;     // far away -> gentle flash interval
constexpr float DIST_NEAR_MM = 10.0f;
constexpr float DIST_FAR_MM  = 1600.0f;
constexpr uint32_t HEARTBEAT_DEFAULT_MS = 500;
constexpr uint32_t HEARTBEAT_JITTER_MS = 10;    // minimum delta before updating interval

uint32_t distanceToHeartbeat(float mm) {
	float clamped = clamp(mm, DIST_NEAR_MM, DIST_FAR_MM);
	float mapped = map(clamped,
		DIST_NEAR_MM,
		DIST_FAR_MM,
		static_cast<float>(HEARTBEAT_MIN_MS),
		static_cast<float>(HEARTBEAT_MAX_MS));
	float bounded = clamp(mapped,
		static_cast<float>(HEARTBEAT_MIN_MS),
		static_cast<float>(HEARTBEAT_MAX_MS));
	return static_cast<uint32_t>(bounded + 0.5f);
}

uint32_t s_lastHeartbeatMs = 0;

} // namespace

namespace HeartbeatPolicy {

void configure() {
	s_lastHeartbeatMs = 0;
}

uint32_t defaultIntervalMs() {
	return HEARTBEAT_DEFAULT_MS;
}

uint32_t clampInterval(uint32_t intervalMs) {
	if (intervalMs < HEARTBEAT_MIN_MS) return HEARTBEAT_MIN_MS;
	if (intervalMs > HEARTBEAT_MAX_MS) return HEARTBEAT_MAX_MS;
	return intervalMs;
}

bool bootstrap(float distanceMm, uint32_t &intervalOut) {
	if (distanceMm <= 0.0f) {
		return false;
	}

	uint32_t next = distanceToHeartbeat(distanceMm);
	s_lastHeartbeatMs = next;
	intervalOut = next;
	HB_LOG("[HeartbeatPolicy] Bootstrap distance %.0fmm -> %lums\n",
		   distanceMm,
		   static_cast<unsigned long>(intervalOut));
	return true;
}

bool intervalFromDistance(float distanceMm, uint32_t &intervalOut) {
	if (distanceMm <= 0.0f) {
		return false;
	}

	uint32_t interval = distanceToHeartbeat(distanceMm);
	if (s_lastHeartbeatMs != 0) {
		const uint32_t diff = (interval > s_lastHeartbeatMs)
		    ? (interval - s_lastHeartbeatMs)
		    : (s_lastHeartbeatMs - interval);
		if (diff < HEARTBEAT_JITTER_MS) {
			return false;
		}
	}

	s_lastHeartbeatMs = interval;
	intervalOut = interval;
	HB_LOG("[HeartbeatPolicy] Distance %.0fmm -> %lums\n",
	   distanceMm,
	   static_cast<unsigned long>(interval));
	return true;
}

} // namespace HeartbeatPolicy
