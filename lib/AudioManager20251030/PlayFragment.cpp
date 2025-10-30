// lib/AudioManager20251022/PlayFragment.cpp
#include "PlayFragment.h"
#include "SDManager.h"
#include "Globals.h"
#include "AudioState.h"
#include "AudioManager.h"
#include "TimerManager.h"
#include <math.h>

#ifndef LOG_FADE
#define LOG_FADE 0
#endif
#if LOG_FADE
    #define FADE_LOG(...) LOG_DEBUG(__VA_ARGS__)
#else
  #define FADE_LOG(...) do {} while (0)
#endif

extern const char* getMP3Path(uint8_t dirIdx, uint8_t fileIdx);

#ifndef FADE_POWER
#define FADE_POWER 2.0f
#endif

namespace {

constexpr uint8_t kFadeSteps = 15;

struct FadeState {
    bool     curveReady = false;
    float    curve[kFadeSteps];
    uint16_t effectiveMs = 0;
    uint16_t stepMs = 1;
    uint32_t fadeOutDelayMs = 0;
    uint8_t  inIndex = 0;
    uint8_t  outIndex = 0;
    uint8_t  lastCurveIndex = 0;
    float    currentFactor = 0.0f;
};

FadeState& fade() {
    static FadeState state;
    return state;
}

AudioManager& audio() {
    return AudioManager::instance();
}

TimerManager& timers() {
    return TimerManager::instance();
}

void ensureCurve() {
    auto& state = fade();
    if (state.curveReady) return;
    float power = FADE_POWER;
    if (power < 1.0f) {
        power = 1.0f;
    }
    for (uint8_t i = 0; i < kFadeSteps; ++i) {
        const float numerator = static_cast<float>(i);
        uint8_t stepCount = 1U;
        if (kFadeSteps > 1U) {
            stepCount = static_cast<uint8_t>(kFadeSteps - 1U);
        }
        const float denominator = static_cast<float>(stepCount);
        float x = 1.0f;
        if (kFadeSteps > 1U) {
            x = numerator / denominator;
        }
        const float s = sinf(1.5707963f * x);
        state.curve[i] = powf(s, power);
    }
    state.curveReady = true;
}

inline void setFadeFactor(float value) {
    fade().currentFactor = value;
}

inline float fadeFactor() {
    return fade().currentFactor;
}

inline float currentGain() {
    return getBaseGain() * fadeFactor() * getWebAudioLevel();
}

inline void applyGain() {
    audio().audioOutput.SetGain(currentGain());
}

void resetFadeIndices() {
    fade().inIndex = 0;
    fade().outIndex = 0;
    fade().lastCurveIndex = 0;
    fade().currentFactor = 0.0f;
}

void stopPlayback();
void handleFadeIn();
void handleFadeOut();
void beginFadeOut();

} // namespace

namespace PlayAudioFragment {

bool start(const AudioFragment& fragment) {
    if (isAudioBusy()) {
        LOG_WARN("[Audio] startFragment ignored: audio busy\n");
        return false;
    }

    ensureCurve();

    auto& state = fade();

    setAudioBusy(true);
    setFragmentPlaying(true);
    setSentencePlaying(false);
    setCurrentDirFile(fragment.dirIndex, fragment.fileIndex);

    uint32_t maxFade = 1;
    if (fragment.durationMs >= 2) {
        maxFade = fragment.durationMs / 2;
    }
    uint32_t requested = fragment.fadeMs;
    if (requested > maxFade) requested = maxFade;
    state.effectiveMs = static_cast<uint16_t>(requested);
    if (state.effectiveMs == 0) {
        state.stepMs = 1;
    } else {
        state.stepMs = static_cast<uint16_t>(state.effectiveMs / kFadeSteps);
    }
    if (state.stepMs == 0) state.stepMs = 1;
    const uint32_t doubleFade = static_cast<uint32_t>(state.effectiveMs) * 2U;
    if (fragment.durationMs > doubleFade) {
        state.fadeOutDelayMs = fragment.durationMs - doubleFade;
    } else {
        state.fadeOutDelayMs = 0;
    }
    resetFadeIndices();

    setFadeFactor(0.0f);
    applyGain();

    const char* path = getMP3Path(fragment.dirIndex, fragment.fileIndex);
    audio().audioFile = new AudioFileSourceSD(path);
    if (!audio().audioFile) {
        LOG_ERROR("[Audio] Failed to allocate source for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    audio().audioMp3Decoder = new AudioGeneratorMP3();
    if (!audio().audioMp3Decoder) {
        LOG_ERROR("[Audio] Failed to allocate MP3 decoder\n");
        stopPlayback();
        return false;
    }

    if (!audio().audioMp3Decoder->begin(audio().audioFile, &audio().audioOutput)) {
        LOG_ERROR("[Audio] Decoder begin failed for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    timers().cancel(beginFadeOut);
    timers().cancel(handleFadeIn);
    timers().cancel(handleFadeOut);

    if (!timers().create(state.stepMs, kFadeSteps, handleFadeIn)) {
        LOG_WARN("[Fade] Failed to start fade-in timer\n");
    }

    if (state.fadeOutDelayMs == 0) {
        if (!timers().create(state.stepMs, kFadeSteps, handleFadeOut)) {
            LOG_WARN("[Fade] Failed to start fade-out timer\n");
        }
    } else {
        if (!timers().create(state.fadeOutDelayMs, 1, beginFadeOut)) {
            LOG_WARN("[Fade] Failed to schedule fade-out delay (%lu ms)\n", static_cast<unsigned long>(state.fadeOutDelayMs));
        }
    }

    FADE_LOG("[Fade] start dur=%lu fadeMs=%u stepMs=%u delay=%lu\n",
             static_cast<unsigned long>(fragment.durationMs),
             static_cast<unsigned>(state.effectiveMs),
             static_cast<unsigned>(state.stepMs),
             static_cast<unsigned long>(state.fadeOutDelayMs));

    return true;
}

constexpr uint16_t kFadeUseCurrent = 0xFFFF;
constexpr uint16_t kFadeMinMs = 40;

void stop(uint16_t fadeOutMs) {
    if (!isAudioBusy()) return;

    auto& state = fade();

    timers().cancel(beginFadeOut);
    timers().cancel(handleFadeOut);
    timers().cancel(handleFadeIn);

    uint16_t effective = fadeOutMs;
    if (effective == kFadeUseCurrent) {
        effective = state.effectiveMs;
    }

    if (effective <= kFadeMinMs || kFadeSteps == 0U) {
        stopPlayback();
        return;
    }

    state.effectiveMs = effective;
    state.fadeOutDelayMs = 0;
    state.stepMs = static_cast<uint16_t>(effective / kFadeSteps);
    if (state.stepMs == 0) {
        state.stepMs = 1;
    }

    uint8_t startOffset = 0;
    if (kFadeSteps > 0U) {
        startOffset = static_cast<uint8_t>((kFadeSteps - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;

    if (!timers().restart(state.stepMs, kFadeSteps, handleFadeOut)) {
        LOG_WARN("[Fade] Failed to restart stop() fade-out timer\n");
        stopPlayback();
    } else {
        FADE_LOG("[Fade] stop() -> fadeOut stepMs=%u overrideMs=%u\n",
                 static_cast<unsigned>(state.stepMs),
                 static_cast<unsigned>(effective));
    }
}

void updateGain() {
    if (!isAudioBusy()) return;
    applyGain();
}

} // namespace PlayAudioFragment

namespace {

void stopPlayback() {
    timers().cancel(beginFadeOut);
    timers().cancel(handleFadeIn);
    timers().cancel(handleFadeOut);

    if (audio().audioMp3Decoder) {
        audio().audioMp3Decoder->stop();
        delete audio().audioMp3Decoder;
        audio().audioMp3Decoder = nullptr;
    }
    if (audio().audioFile) {
        delete audio().audioFile;
        audio().audioFile = nullptr;
    }

    setFadeFactor(0.0f);
    applyGain();

    setAudioBusy(false);
    setFragmentPlaying(false);
    setSentencePlaying(false);

    fade().effectiveMs = 0;
    fade().stepMs = 1;
    fade().fadeOutDelayMs = 0;
    resetFadeIndices();

    audio().updateGain();

    FADE_LOG("[Fade] stopped\n");
}

void handleFadeIn() {
    auto& state = fade();
    uint8_t idx = 0;
    if (kFadeSteps > 0U) {
        idx = static_cast<uint8_t>(kFadeSteps - 1U);
    }
    if (state.inIndex < kFadeSteps) {
        idx = state.inIndex;
    }
    setFadeFactor(state.curve[idx]);
    applyGain();
    state.lastCurveIndex = idx;
    FADE_LOG("[FadeIn] idx=%u factor=%.3f gain=%.3f\n", idx, fadeFactor(), currentGain());

    state.inIndex++;
    if (state.inIndex >= kFadeSteps) {
        timers().cancel(handleFadeIn);
        state.inIndex = 0;
        FADE_LOG("[FadeIn] done\n");
    }
}

void handleFadeOut() {
    auto& state = fade();
    uint8_t idx = 0;
    if (state.outIndex < kFadeSteps) {
        idx = static_cast<uint8_t>((kFadeSteps - 1U) - state.outIndex);
    }
    setFadeFactor(state.curve[idx]);
    applyGain();
    state.lastCurveIndex = idx;
    FADE_LOG("[FadeOut] idx=%u factor=%.3f gain=%.3f\n", idx, fadeFactor(), currentGain());

    state.outIndex++;
    if (state.outIndex >= kFadeSteps) {
        timers().cancel(handleFadeOut);
        state.outIndex = 0;
        FADE_LOG("[FadeOut] done -> stop\n");
        stopPlayback();
    }
}

void beginFadeOut() {
    auto& state = fade();
    uint8_t startOffset = 0;
    if (kFadeSteps > 0U) {
        startOffset = static_cast<uint8_t>((kFadeSteps - 1U) - state.lastCurveIndex);
    }
    state.outIndex = startOffset;
    timers().cancel(handleFadeOut);
    uint16_t step = state.stepMs;
    if (step == 0) {
        step = 1;
    }
    if (!timers().create(step, kFadeSteps, handleFadeOut)) {
        LOG_WARN("[Fade] Failed to launch delayed fade-out timer\n");
        stopPlayback();
    } else {
        FADE_LOG("[Fade] delayed fade-out started (step=%u)\n", static_cast<unsigned>(step));
    }
}

} // namespace

void PlayAudioFragment::abortImmediate() {
    stopPlayback();
}
