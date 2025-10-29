# Audio Conduct – Distance Playback Notes

This file captures the current understanding of how distance driven PCM playback is coordinated with the rest of the audio stack. It is mostly here as a reminder for future maintenance sessions.

## Roles
- **AudioPolicy** decides if a distance reading should produce audio at all and exposes any ancillary knobs (volume shaping, etc.).
- **AudioConduct** interprets policy results, keeps the most recent distance measurement, and schedules PCM playback attempts through the timer layer using the policy-derived cadence (when policy declines we park the timer far in the future). The distance clip pointer is registered once during boot via `setDistanceClipPointer()` and read with `getDistanceClipPointer()` wherever needed.
- **AudioManager** exposes the live playback state (fragments, sentences, PCM clips) and executes stop/start requests.
- **TimerManager** owns the actual timers; `AudioConduct` never runs its own scheduler, it only asks `TimerManager::restart` to re-arm the single callback.

## PCM vs fragment sequencing
1. When a new distance reading arrives, the sensor conductor checks `AudioPolicy::distancePlaybackInterval` purely for eligibility. If policy declines we park the timer with a long interval; otherwise we call `AudioConduct::startDistanceResponse` once on entry (and again after each playback completes).
2. `startDistanceResponse` requests a fragment stop when needed and arms the timer using `AudioPolicy::distancePlaybackInterval`; when policy refuses we set a one-megasecond parking interval. The callback will pick the distance up again through `SensorsPolicy` when it finally fires.

## Timer usage
- `AudioConduct` calls `TimerManager::restart` directly whenever a distance response is started or refreshed. There is a single timer slot for the callback; if the restart fails we log it and abort the attempt.
- We do not keep side-band timer state; the manager remains the single source of truth.

## Policy requirements
- `AudioPolicy::distancePlaybackInterval` provides both the eligibility decision and the cadence. If the policy rejects the reading we park the timer at one million milliseconds and wait for a fresh distance to re-arm it.
- If a fragment is still winding down the first PCM request is refused; the timer-driven retries are the sanctioned way to wait until audio is free.
- After a fragment stop is issued the next distance playback always restarts from the top; there is no attempt to resume an old clip.

## Callback behaviour
- `cb_playPCM()` checks whether other audio is still busy. If fragments or sentences are active it re-arms itself with the busy retry window; otherwise it fetches the latest cached distance via `SensorsPolicy::currentDistance`, refreshes volume through `AudioPolicy::updateDistancePlaybackVolume`, and starts PCM playback.
- Actual playback is attempted via `PlayPCM::play`. A failure here is logged but otherwise the logic relies on the next distance event (or a fresh `setDistanceClipPointer()` call) to recover.

These notes should keep the interaction between policy, conduct, and the timers straight without re-learning the same rules next time.
