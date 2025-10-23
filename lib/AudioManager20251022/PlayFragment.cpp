// lib/AudioManager20251022/PlayFragment.cpp
#include "PlayFragment.h"
#include "SDManager.h"
#include "Globals.h"
#include "AudioManager.h"
#include "TimerManager.h"
#include <math.h>

#ifndef LOG_FADE
#define LOG_FADE 0
#endif
#if LOG_FADE
  #ifdef PF
    #define FADE_LOG(...) PF(__VA_ARGS__)
  #else
    #define FADE_LOG(...) do { Serial.printf(__VA_ARGS__); } while (0)
  #endif
#else
  #define FADE_LOG(...) do {} while (0)
#endif

extern const char* getMP3Path(uint8_t dirIdx, uint8_t fileIdx);
extern void        setFFade(float v);
extern float       getFFade();
extern float       getBaseGain();
extern float       getWebAudioLevel();
extern bool        isAudioBusy();
extern void        setAudioBusy(bool b);
extern void        setFragmentPlaying(bool b);
extern void        setSentencePlaying(bool b);
extern void        setCurrentDirFile(uint8_t dir, uint8_t file);

#ifndef FADE_POWER
#define FADE_POWER 2.0f
#endif

namespace {

constexpr uint8_t kFadeSteps = MAX_FADE_STEPS;

struct FadeState {
    bool     curveReady = false;
    float    curve[kFadeSteps];
    uint16_t effectiveMs = 0;
    uint16_t stepMs = 1;
    uint32_t fadeOutDelayMs = 0;
    uint8_t  inIndex = 0;
    uint8_t  outIndex = 0;
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
    const float power = (FADE_POWER < 1.0f) ? 1.0f : FADE_POWER;
    for (uint8_t i = 0; i < kFadeSteps; ++i) {
        const float x = (kFadeSteps > 1) ? (float)i / (float)(kFadeSteps - 1) : 1.0f;
        const float s = sinf(1.5707963f * x);
        state.curve[i] = powf(s, power);
    }
    state.curveReady = true;
}

inline float currentGain() {
    return getBaseGain() * getFFade() * getWebAudioLevel();
}

inline void applyGain() {
    audio().audioOutput.SetGain(currentGain());
}

void resetFadeIndices() {
    fade().inIndex = 0;
    fade().outIndex = 0;
}

void stopPlayback();
void handleFadeIn();
void handleFadeOut();
void beginFadeOut();

} // namespace

namespace PlayAudioFragment {

bool start(const AudioFragment& fragment) {
    if (isAudioBusy()) {
        PF("[Fade] Start ignored: audio busy\n");
        return false;
    }

    ensureCurve();

    auto& state = fade();

    setAudioBusy(true);
    setFragmentPlaying(true);
    setSentencePlaying(false);
    setCurrentDirFile(fragment.dirIndex, fragment.fileIndex);

    const uint32_t maxFade = (fragment.durationMs >= 2) ? (fragment.durationMs / 2) : 1;
    uint32_t requested = fragment.fadeMs;
    if (requested > maxFade) requested = maxFade;
    state.effectiveMs = (uint16_t)requested;
    state.stepMs = (state.effectiveMs == 0) ? 1 : (uint16_t)(state.effectiveMs / kFadeSteps);
    if (state.stepMs == 0) state.stepMs = 1;
    state.fadeOutDelayMs = (fragment.durationMs > state.effectiveMs) ? (fragment.durationMs - state.effectiveMs) : 0;
    resetFadeIndices();

    setFFade(0.0f);
    applyGain();

    const char* path = getMP3Path(fragment.dirIndex, fragment.fileIndex);
    audio().audioFile = new AudioFileSourceSD(path);
    if (!audio().audioFile) {
        PF("[Audio] Failed to allocate source for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    audio().audioMp3Decoder = new AudioGeneratorMP3();
    if (!audio().audioMp3Decoder) {
        PF("[Audio] Failed to allocate MP3 decoder\n");
        stopPlayback();
        return false;
    }

    if (!audio().audioMp3Decoder->begin(audio().audioFile, &audio().audioOutput)) {
        PF("[Audio] Decoder begin failed for %03u/%03u\n", fragment.dirIndex, fragment.fileIndex);
        stopPlayback();
        return false;
    }

    timers().cancel(beginFadeOut);
    timers().cancel(handleFadeIn);
    timers().cancel(handleFadeOut);

    if (!timers().create(state.stepMs, kFadeSteps, handleFadeIn)) {
        PF("[Fade] Failed to start fade-in timer\n");
    }

    if (state.fadeOutDelayMs == 0) {
        if (!timers().create(state.stepMs, kFadeSteps, handleFadeOut)) {
            PF("[Fade] Failed to start fade-out timer\n");
        }
    } else {
        if (!timers().create(state.fadeOutDelayMs, 1, beginFadeOut)) {
            PF("[Fade] Failed to schedule fade-out delay (%lu ms)\n", (unsigned long)state.fadeOutDelayMs);
        }
    }

    FADE_LOG("[Fade] start dur=%lu fadeMs=%u stepMs=%u delay=%lu\n",
             (unsigned long)fragment.durationMs,
             (unsigned)state.effectiveMs,
             (unsigned)state.stepMs,
             (unsigned long)state.fadeOutDelayMs);

    return true;
}

void stop() {
    if (!isAudioBusy()) return;

    auto& state = fade();

    timers().cancel(beginFadeOut);
    timers().cancel(handleFadeOut);
    timers().cancel(handleFadeIn);

    state.outIndex = 0;
    const uint16_t step = state.stepMs ? state.stepMs : 1;
    if (!timers().create(step, kFadeSteps, handleFadeOut)) {
        PF("[Fade] Failed to start stop() fade-out timer\n");
        stopPlayback();
    } else {
        FADE_LOG("[Fade] stop() -> fadeOut stepMs=%u\n", (unsigned)step);
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

    setFFade(0.0f);
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
    const uint8_t idx = (state.inIndex < kFadeSteps) ? state.inIndex : (kFadeSteps - 1);
    setFFade(state.curve[idx]);
    applyGain();
    FADE_LOG("[FadeIn] idx=%u factor=%.3f gain=%.3f\n", idx, getFFade(), currentGain());

    state.inIndex++;
    if (state.inIndex >= kFadeSteps) {
        timers().cancel(handleFadeIn);
        state.inIndex = 0;
        FADE_LOG("[FadeIn] done\n");
    }
}

void handleFadeOut() {
    auto& state = fade();
    const uint8_t idx = (state.outIndex < kFadeSteps)
        ? (uint8_t)((kFadeSteps - 1) - state.outIndex)
        : 0;
    setFFade(state.curve[idx]);
    applyGain();
    FADE_LOG("[FadeOut] idx=%u factor=%.3f gain=%.3f\n", idx, getFFade(), currentGain());

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
    state.outIndex = 0;
    timers().cancel(handleFadeOut);
    const uint16_t step = state.stepMs ? state.stepMs : 1;
    if (!timers().create(step, kFadeSteps, handleFadeOut)) {
        PF("[Fade] Failed to launch delayed fade-out timer\n");
        stopPlayback();
    } else {
        FADE_LOG("[Fade] delayed fade-out started (step=%u)\n", (unsigned)step);
    }
}

} // namespace

bool getRandomFragment(AudioFragment& outFrag) {
    DirEntry dirEntry;
    uint32_t totalDirScore = 0;
    uint8_t  validDirs[SD_MAX_DIRS];
    uint32_t dirScores[SD_MAX_DIRS];
    uint8_t  dirCount = 0;

    File rootFile = SD.open(ROOT_DIRS, FILE_READ);
    if (!rootFile) {
        PF("[FragSelect] Can't open %s\n", ROOT_DIRS);
        return false;
    }

    auto& sdMgr = SDManager::instance();
    for (uint16_t dirNum = 1; dirNum <= sdMgr.getHighestDirNum(); dirNum++) {
        const uint32_t offset = (uint32_t)(dirNum - 1) * (uint32_t)sizeof(DirEntry);
        if (!rootFile.seek(offset)) continue;
        if (rootFile.read((uint8_t*)&dirEntry, sizeof(DirEntry)) != sizeof(DirEntry)) continue;

        if (dirEntry.total_score > 0 && dirEntry.file_count > 0) {
            if (dirCount >= SD_MAX_DIRS) break;
            validDirs[dirCount] = (uint8_t)dirNum;
            dirScores[dirCount] = (uint32_t)dirEntry.total_score;
            totalDirScore += (uint32_t)dirEntry.total_score;
            dirCount++;
        }
    }
    rootFile.close();

    if (dirCount == 0 || totalDirScore == 0) {
        PF("[FragSelect] No valid dirs\n");
        return false;
    }

    uint32_t pick = (uint32_t)random((long)totalDirScore) + 1;
    uint32_t sum = 0;
    uint8_t chosenDir = validDirs[dirCount - 1];
    for (uint8_t i = 0; i < dirCount; i++) {
        sum += dirScores[i];
        if (pick <= sum) { chosenDir = validDirs[i]; break; }
    }

    FileEntry fileEntry;
    uint32_t totalFileScore = 0;
    uint8_t  validFiles[SD_MAX_FILES_PER_SUBDIR];
    uint32_t fileScores[SD_MAX_FILES_PER_SUBDIR];
    uint8_t  fileCount = 0;

    for (uint16_t fileNum = 1; fileNum <= SD_MAX_FILES_PER_SUBDIR; fileNum++) {
        if (!sdMgr.readFileEntry(chosenDir, fileNum, &fileEntry)) continue;
        if (fileEntry.score > 0 && fileEntry.size_kb > 0) {
            if (fileCount >= SD_MAX_FILES_PER_SUBDIR) break;
            validFiles[fileCount] = (uint8_t)fileNum;
            fileScores[fileCount] = (uint32_t)fileEntry.score;
            totalFileScore += (uint32_t)fileEntry.score;
            fileCount++;
        }
    }

    if (fileCount == 0 || totalFileScore == 0) {
        PF("[FragSelect] No valid files in dir %03u\n", chosenDir);
        return false;
    }

    uint32_t filePick = (uint32_t)random((long)totalFileScore) + 1;
    sum = 0;
    uint8_t chosenFile = validFiles[fileCount - 1];
    for (uint8_t i = 0; i < fileCount; i++) {
        sum += fileScores[i];
        if (filePick <= sum) { chosenFile = validFiles[i]; break; }
    }

    if (!sdMgr.readFileEntry(chosenDir, chosenFile, &fileEntry)) return false;

    const uint32_t rawDuration = (uint32_t)fileEntry.size_kb * 1024UL / BYTES_PER_MS;
    if (rawDuration <= HEADER_MS + 100) {
        PF("[FragSelect] Too short: dir %03u file %03u\n", chosenDir, chosenFile);
        return false;
    }

    outFrag.dirIndex   = chosenDir;
    outFrag.fileIndex  = chosenFile;
    outFrag.startMs    = HEADER_MS;
    outFrag.durationMs = rawDuration - HEADER_MS;
    outFrag.fadeMs     = 5000;

    return true;
}
