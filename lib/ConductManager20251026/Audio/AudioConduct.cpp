#include "AudioConduct.h"

#include <Arduino.h>

#include "AudioManager.h"
#include "PlayPCM.h"
#include "PlayFragment.h"
#include "AudioPolicy.h"
#include "Globals.h"
#include "TimerManager.h"
#include "../Sensors/SensorsPolicy.h"

#ifndef AUDIO_CONDUCT_DEBUG
#define AUDIO_CONDUCT_DEBUG 0
#endif

#if AUDIO_CONDUCT_DEBUG
#define AC_LOG(...) PF(__VA_ARGS__)
#else
#define AC_LOG(...) do {} while (0)
#endif

const char* const AudioConduct::kDistanceClipId = "distance_ping";

namespace {

constexpr float kPlaybackVolume = 0.6f;
constexpr uint32_t kBusyRetryMs = 120;

const AudioManager::PCMClipDesc* s_distanceClip = nullptr;

AudioManager& audio() {
    return AudioManager::instance();
}

TimerManager& timers() {
    return TimerManager::instance();
}

void cb_playPCM() {
    auto& mgr = audio();
    if (mgr.isFragmentPlaying() || mgr.isSentencePlaying()) {
        AC_LOG("[AudioConduct] Playback deferred (audio busy), retry in %lu ms\n",
               static_cast<unsigned long>(kBusyRetryMs));
        timers().restart(kBusyRetryMs, 1, cb_playPCM);
        return;
    }

    const float distanceMm = SensorsPolicy::currentDistance();

    AudioPolicy::updateDistancePlaybackVolume(distanceMm);

    if (!s_distanceClip || !s_distanceClip->samples) {
        PF("[AudioConduct] Distance clip missing at playback time\n");
        return;
    }

    AC_LOG("[AudioConduct] Triggering distance PCM (distance=%.1fmm)\n",
           static_cast<double>(distanceMm));
    if (!PlayPCM::play(s_distanceClip, kPlaybackVolume)) {
        PF("[AudioConduct] Failed to start distance PCM playback\n");
    }
}

} // namespace

void AudioConduct::plan() {
    timers().cancel(cb_playPCM);

    if (!s_distanceClip || !s_distanceClip->samples) {
        PF("[Conduct][Plan] Distance clip '%s' not available\n", kDistanceClipId);
    } else {
        PF("[Conduct][Plan] Distance playback ready with clip %s\n", kDistanceClipId);
    }
}

void AudioConduct::startDistanceResponse() {
    auto& tm = timers();

    auto& mgr = audio();
    if (mgr.isFragmentPlaying()) {
        PlayAudioFragment::stop();
    }

    uint32_t intervalMs = 1;
    if (s_distanceClip && s_distanceClip->durationMs > 0) {
        intervalMs = s_distanceClip->durationMs;
    }

    const float distanceMm = SensorsPolicy::currentDistance();
    AC_LOG("[AudioConduct] Distance response scheduled (distance=%.1fmm, interval=%lu ms)\n",
           static_cast<double>(distanceMm),
           static_cast<unsigned long>(intervalMs));

    if (!tm.restart(intervalMs, 1, cb_playPCM)) {
        PF("[AudioConduct] Failed to schedule distance playback (%lu ms)\n",
           static_cast<unsigned long>(intervalMs));
    }
}


void AudioConduct::setDistanceClip(const AudioManager::PCMClipDesc* clip) {
    if (!clip || !clip->samples || clip->sampleRate == 0) {
        PF("[AudioConduct] Rejected invalid distance clip registration\n");
        return;
    }

    s_distanceClip = clip;

    PF("[AudioConduct] Distance clip registered (%lu Hz, %lu ms)\n",
       static_cast<unsigned long>(s_distanceClip->sampleRate),
       static_cast<unsigned long>(s_distanceClip->durationMs));

    const float distanceMm = SensorsPolicy::currentDistance();
    uint32_t dummyInterval = 0;
    if (AudioPolicy::distancePlaybackInterval(distanceMm, dummyInterval)) {
        if (!timers().isActive(cb_playPCM)) {
            startDistanceResponse();
        }
    }
}

void AudioConduct::silenceDistance() {
    timers().cancel(cb_playPCM);
}

bool AudioConduct::isDistancePlaybackScheduled() {
    return timers().isActive(cb_playPCM);
}

