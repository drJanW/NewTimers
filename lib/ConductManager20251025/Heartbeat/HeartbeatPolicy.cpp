#include "HeartbeatPolicy.h"

#include "Globals.h"
#include <math.h>

namespace {

#if HEARTBEAT_DEBUG
#define HB_LOG(...) PF(__VA_ARGS__)
#else
#define HB_LOG(...) do {} while (0)
#endif

constexpr uint32_t HEARTBEAT_MIN_MS = 50;       // Fast limit ≈20 Hz when someone is close
constexpr uint32_t HEARTBEAT_MAX_MS = 5000;     // Slow limit ≈0.2 Hz when no one nearby
constexpr float HEARTBEAT_MAX_HZ = 1000.0f / static_cast<float>(HEARTBEAT_MIN_MS);
constexpr float HEARTBEAT_MIN_HZ = 1000.0f / static_cast<float>(HEARTBEAT_MAX_MS);
constexpr float DIST_NEAR_MM = 100.0f;          // Treat 10 cm as "right on top"
constexpr float DIST_FAR_MM  = 1000.0f;         // Beyond 1 m slows to lazy heartbeat
constexpr uint32_t HEARTBEAT_UPDATE_EPSILON_MS = 20; // Require meaningful change before rescheduling
constexpr float DIST_SMOOTH_ALPHA = 0.35f;      // Simple low-pass filter for sensor jitter
constexpr float DIST_RAPID_THRESHOLD_MM = 80.0f; // Jump distance that resets smoothing immediately
constexpr uint32_t HEARTBEAT_DEFAULT_MS = 500;  // Mid-range beat when no input yet

float s_filteredDistanceMm = 0.0f;
uint32_t s_lastHeartbeatMs = 0;

float clampFloat(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

uint32_t distanceToHeartbeat(float mm) {
	float clamped = clampFloat(mm, DIST_NEAR_MM, DIST_FAR_MM);
	float ratio = (clamped - DIST_NEAR_MM) / (DIST_FAR_MM - DIST_NEAR_MM);
	float freqRange = HEARTBEAT_MAX_HZ - HEARTBEAT_MIN_HZ;
	float freq = HEARTBEAT_MAX_HZ - ratio * freqRange;
	freq = clampFloat(freq, HEARTBEAT_MIN_HZ, HEARTBEAT_MAX_HZ);
	float interval = 1000.0f / (freq > 0.01f ? freq : 0.01f);
	if (interval < static_cast<float>(HEARTBEAT_MIN_MS)) interval = static_cast<float>(HEARTBEAT_MIN_MS);
	if (interval > static_cast<float>(HEARTBEAT_MAX_MS)) interval = static_cast<float>(HEARTBEAT_MAX_MS);
	return static_cast<uint32_t>(roundf(interval));
}

} // namespace

namespace HeartbeatPolicy {

void configure() {
	s_filteredDistanceMm = 0.0f;
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
	if (s_lastHeartbeatMs != 0) {
		intervalOut = s_lastHeartbeatMs;
		return false;
	}
	s_filteredDistanceMm = distanceMm;
	s_lastHeartbeatMs = distanceToHeartbeat(distanceMm);
	intervalOut = s_lastHeartbeatMs;
	HB_LOG("[HeartbeatPolicy] Bootstrap distance %.0fmm -> %lums\n",
		   distanceMm,
		   static_cast<unsigned long>(intervalOut));
	return true;
}

bool intervalFromDistance(float distanceMm, uint32_t &intervalOut) {
	if (distanceMm <= 0.0f) {
		return false;
	}

	if (s_filteredDistanceMm <= 0.0f) {
		s_filteredDistanceMm = distanceMm;
	} else {
		float delta = fabsf(distanceMm - s_filteredDistanceMm);
		if (delta >= DIST_RAPID_THRESHOLD_MM) {
			s_filteredDistanceMm = distanceMm;
		} else {
			s_filteredDistanceMm += (distanceMm - s_filteredDistanceMm) * DIST_SMOOTH_ALPHA;
		}
	}

	uint32_t interval = distanceToHeartbeat(s_filteredDistanceMm);
	uint32_t diff = (interval > s_lastHeartbeatMs)
						? (interval - s_lastHeartbeatMs)
						: (s_lastHeartbeatMs - interval);

	if (s_lastHeartbeatMs == 0 || diff >= HEARTBEAT_UPDATE_EPSILON_MS) {
		s_lastHeartbeatMs = interval;
		intervalOut = interval;
		HB_LOG("[HeartbeatPolicy] Distance %.0fmm (%.0fmm smoothed) -> %lums\n",
			   distanceMm,
			   s_filteredDistanceMm,
			   static_cast<unsigned long>(interval));
		return true;
	}

	return false;
}

} // namespace HeartbeatPolicy
