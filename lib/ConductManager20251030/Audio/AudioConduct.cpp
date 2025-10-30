#include "AudioConduct.h"

#include <Arduino.h>
#include <atomic>

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
#define AC_LOG(...) \
    do              \
    {               \
    } while (0)
#endif

const char *const AudioConduct::kDistanceClipId = "distance_ping";

namespace
{

    constexpr float kPlaybackVolume = 0.6f;
    constexpr uint32_t kBusyRetryMs = 120;

    std::atomic<const AudioManager::PCMClipDesc *> s_distanceClip{nullptr};

    AudioManager &audio()
    {
        return AudioManager::instance();
    }

    TimerManager &timers()
    {
        return TimerManager::instance();
    }

} // namespace

// boot registers the clip once; later calls simply overwrite the atomic pointer
void setDistanceClipPointer(const AudioManager::PCMClipDesc *clip)
{
    setMux(clip, &s_distanceClip);
}

// call sites expect a valid clip because startDistanceResponse refuses to arm otherwise
const AudioManager::PCMClipDesc *getDistanceClipPointer()
{
    return getMux(&s_distanceClip);
}

void AudioConduct::cb_playPCM()
{

    // timer only fires after boot set the clip; we bail early if audio stack is busy

    auto &mgr = audio();
    if (mgr.isFragmentPlaying() || mgr.isSentencePlaying())
    {
        timers().restart(kBusyRetryMs, 1, AudioConduct::cb_playPCM);
        return;
    }

    const float distanceMm = SensorsPolicy::currentDistance();

    AudioPolicy::updateDistancePlaybackVolume(distanceMm);

    // playback failure just logs; policy interval logic will try again on the next timer pass
    AC_LOG("[AudioConduct] Triggering distance PCM (distance=%.1fmm)\n",
           static_cast<double>(distanceMm));
    if (!PlayPCM::play(getDistanceClipPointer(), kPlaybackVolume))
    {
        PF("[AudioConduct] Failed to start distance PCM playback\n");
    }

    startDistanceResponse();
}

void AudioConduct::plan()
{
    timers().cancel(AudioConduct::cb_playPCM);
    PF("[Conduct][Plan] Distance playback ready with clip %s\n", kDistanceClipId);
}

void AudioConduct::startDistanceResponse()
{
    // if boot never set the clip we skip scheduling entirely
    if (!getDistanceClipPointer())
    {
        return;
    }

    const float distanceMm = SensorsPolicy::currentDistance();

    uint32_t intervalMs = 0;
    const bool policyAllowsPlayback = AudioPolicy::distancePlaybackInterval(distanceMm, intervalMs);
    if (!policyAllowsPlayback)
    {
        intervalMs = 1000 * 60 * 60 * 1000U; // park the timer far in the future when policy declines
    }

    auto &tm = timers();

    auto &mgr = audio();
    // fragments fade out before distance pings; stop using existing fade behaviour
    if (mgr.isFragmentPlaying())
    {
        const uint16_t fadeMs = static_cast<uint16_t>(clamp(intervalMs, 100U, 5000U));
        PlayAudioFragment::stop(fadeMs);
    }
    AC_LOG("[AudioConduct] Distance response scheduled (distance=%.1fmm, interval=%lu ms)\n",
           static_cast<double>(distanceMm),
           static_cast<unsigned long>(intervalMs));

    if (!tm.restart(intervalMs, 1, AudioConduct::cb_playPCM))
    {
        PF("[AudioConduct] Failed to schedule distance playback (%lu ms)\n",
           static_cast<unsigned long>(intervalMs));
    }
}

// no need for additional helpers; pointer is wired once during boot
