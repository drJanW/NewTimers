// lib/AudioManager20251005/PlayFragment.cpp
#include "PlayFragment.h"
#include "SDManager.h"
#include "Globals.h"     // MAX_FADE_STEPS, BYTES_PER_MS, ROOT_DIRS, HEADER_MS, PF()
#include "AudioManager.h"
#include "TimerManager.h"
#include <math.h>

// ===== Logging =====
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

// ===== Externals uit Globals.h =====
extern const char*  getMP3Path(uint8_t dirIdx, uint8_t fileIdx);
extern void         setFFade(float v);
extern float        getFFade();
extern float        getBaseGain();
extern float        getWebAudioLevel();
extern bool         isAudioBusy();
extern void         setAudioBusy(bool b);
extern void         setFragmentPlaying(bool b);
extern void         setSentencePlaying(bool b);

// ===== Fade-curve =====
#ifndef FADE_POWER
#define FADE_POWER 2.0f   // “sin*sin”
#endif

namespace PlayAudioFragment {

static AudioManager& audio() { return AudioManager::instance(); }
static TimerManager& timerMgr() { return TimerManager::instance(); }

// ===== lokale state =====
static float    fadeValues[MAX_FADE_STEPS];
static uint16_t effFadeMs     = 0;     // effectieve fade na cap
static uint16_t fadeStepIntervalMs = 1; // bewaart huidige stapgrootte voor fade timers

// TimerManager beheert het aantal callbacks via repeat-count; wij tellen lokaal alleen indexen
static uint8_t  k_in          = 0;
static uint8_t  k_out         = 0;

// dt-logging
static uint32_t g_in_t0  = 0, g_in_last  = 0;
static uint32_t g_out_t0 = 0, g_out_last = 0;

// fwd
static void onFadeInStep();
static void onFadeOutStep();
static void stopPlayback();
static void scheduleFadeOutStart();

static inline float currentGain() { return getBaseGain() * getFFade() * getWebAudioLevel(); }
static inline void  applyGain()   { audio().audioOutput.SetGain(currentGain()); }

// ===== fade tabel opbouwen =====
void prepareFadeArray(uint8_t steps) {
  const float k = (FADE_POWER < 1.0f) ? 1.0f : FADE_POWER;
  for (uint8_t i = 0; i < steps; ++i) {
    const float x = (float)i / (float)(steps - 1); // 0..1
    const float s = sinf(1.5707963f * x);          // 0..1
    fadeValues[i] = powf(s, k);                    // sin^k
  }
}

// ===== start fragment + fades =====
void start(uint8_t dirIndex, uint8_t fileIndex, uint32_t startMs, uint32_t durationMs, uint16_t fadeMs)
{
  if (isAudioBusy()) return;

  setAudioBusy(true);
  setFragmentPlaying(true);
  setCurrentDirFile(dirIndex, fileIndex);


  // cap: fade nooit langer dan halve duur
  const uint32_t maxFade = (durationMs >= 2) ? (durationMs / 2) : 1;
  effFadeMs = (fadeMs > maxFade) ? (uint16_t)maxFade : fadeMs;
  const uint16_t stepMs = (uint16_t)((effFadeMs == 0) ? 1 : ((effFadeMs / MAX_FADE_STEPS) ? (effFadeMs / MAX_FADE_STEPS) : 1));
  const uint32_t t0Ms   = (durationMs > effFadeMs) ? (durationMs - effFadeMs) : 0;

  // audio start
  prepareFadeArray(MAX_FADE_STEPS);
  setFFade(0.0f);
  applyGain(); // stil starten

  const char* path = getMP3Path(dirIndex, fileIndex);
  audio().audioFile       = new AudioFileSourceSD(path);
  audio().audioMp3Decoder = new AudioGeneratorMP3();
  audio().audioMp3Decoder->begin(audio().audioFile, &audio().audioOutput);
  timerMgr().cancel(scheduleFadeOutStart);
  timerMgr().cancel(onFadeInStep);
  timerMgr().cancel(onFadeOutStep);

  // indices + dt reset
  k_in = k_out = 0;
  g_in_t0 = g_in_last = 0;
  g_out_t0 = g_out_last = 0;
  fadeStepIntervalMs = stepMs;

  // correcte API: setTimer(delayMs, intervalMs, repeatCount, callback)
  // totaal 15 callbacks → repeatCount = 14
  if (!timerMgr().create(stepMs, MAX_FADE_STEPS, onFadeInStep)) {
    PF("[Fade] Failed to start fade-in timer\n");
  }

  if (t0Ms == 0) {
  if (!timerMgr().create(stepMs, MAX_FADE_STEPS, onFadeOutStep)) {
      PF("[Fade] Failed to start immediate fade-out timer\n");
    }
  } else {
  if (!timerMgr().create(t0Ms, 1, scheduleFadeOutStart)) {
      PF("[Fade] Failed to schedule fade-out delay (%lu ms)\n", (unsigned long)t0Ms);
  if (!timerMgr().create(stepMs, MAX_FADE_STEPS, onFadeOutStep)) {
        PF("[Fade] Fallback fade-out timer also failed\n");
      }
    }
  }

  FADE_LOG("[Fade] start dur=%lu fadeMs=%u stepMs=%u t0Ms=%lu\n",
           (unsigned long)durationMs, (unsigned)effFadeMs,
           (unsigned)stepMs, (unsigned long)t0Ms);
}

// ===== callbacks: exact één stap per call + dt-logging =====
static void onFadeInStep()
{
  const uint32_t now = millis();
  if (!g_in_t0) g_in_t0 = now;
  if (g_in_last) FADE_LOG("[FadeIn] dt=%lu ms\n", (unsigned long)(now - g_in_last));
  g_in_last = now;

  const uint8_t idx = (k_in < MAX_FADE_STEPS) ? k_in : (MAX_FADE_STEPS - 1);
  setFFade(fadeValues[idx]);
  applyGain();
  FADE_LOG("[FadeIn] idx=%u factor=%.3f gain=%.3f\n", idx, getFFade(), currentGain());

  if (++k_in >= MAX_FADE_STEPS) {
    const uint32_t total = now - g_in_t0;
    FADE_LOG("[FadeIn] done total=%lu ms\n", (unsigned long)total);
  timerMgr().cancel(onFadeInStep);
    k_in = 0;
    g_in_t0 = g_in_last = 0;
  }
}

static void onFadeOutStep()
{
  const uint32_t now = millis();
  if (!g_out_t0) g_out_t0 = now;
  if (g_out_last) FADE_LOG("[FadeOut] dt=%lu ms\n", (unsigned long)(now - g_out_last));
  g_out_last = now;

  const uint8_t j = (k_out < MAX_FADE_STEPS) ? (uint8_t)((MAX_FADE_STEPS - 1) - k_out) : 0;
  setFFade(fadeValues[j]);
  applyGain();
  FADE_LOG("[FadeOut] idx=%u factor=%.3f gain=%.3f\n", j, getFFade(), currentGain());

  if (++k_out >= MAX_FADE_STEPS) {
    const uint32_t total = now - g_out_t0;
    FADE_LOG("[FadeOut] done total=%lu ms -> stop\n", (unsigned long)total);
  timerMgr().cancel(onFadeOutStep);
    k_out = 0;
    g_out_t0 = g_out_last = 0;
    stopPlayback();
  }
}

static void scheduleFadeOutStart()
{
  k_out = 0;
  g_out_t0 = g_out_last = 0;
  timerMgr().cancel(onFadeOutStep);
  if (!timerMgr().create(fadeStepIntervalMs ? fadeStepIntervalMs : 1, MAX_FADE_STEPS, onFadeOutStep)) {
    PF("[Fade] Failed to launch delayed fade-out timer\n");
  }
}

// ===== stoppen =====
static void stopPlayback()
{
  if (audio().audioMp3Decoder) { audio().audioMp3Decoder->stop(); delete audio().audioMp3Decoder; audio().audioMp3Decoder = nullptr; }
  if (audio().audioFile)       { delete audio().audioFile;        audio().audioFile = nullptr; }

  timerMgr().cancel(scheduleFadeOutStart);
  timerMgr().cancel(onFadeInStep);
  timerMgr().cancel(onFadeOutStep);

  setFFade(0.0f);
  applyGain();

  setAudioBusy(false);
  setFragmentPlaying(false);
  setSentencePlaying(false);

  FADE_LOG("[Fade] stopped\n");
}

void stop()
{
  timerMgr().cancel(scheduleFadeOutStart);
  timerMgr().cancel(onFadeInStep);
  timerMgr().cancel(onFadeOutStep);

  const uint16_t stepMs = (uint16_t)((effFadeMs == 0) ? 1 : ((effFadeMs / MAX_FADE_STEPS) ? (effFadeMs / MAX_FADE_STEPS) : 1));
  k_out = 0; g_out_t0 = g_out_last = 0;
  fadeStepIntervalMs = stepMs;
  if (!timerMgr().create(stepMs, MAX_FADE_STEPS, onFadeOutStep)) {
    PF("[Fade] Failed to start stop() fade-out timer\n");
  }
  FADE_LOG("[Fade] stop() -> fadeOut stepMs=%u\n", (unsigned)stepMs);
}

// compat
void updateGain() { applyGain(); }

} // namespace PlayAudioFragment



// ===== Fragment-selectie =====
bool getRandomFragment(AudioFragment& outFrag)
{
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
      totalDirScore      += (uint32_t)dirEntry.total_score;
      dirCount++;
    }
  }
  rootFile.close();

  if (dirCount == 0 || totalDirScore == 0) {
    PF("[FragSelect] No valid dirs\n");
    return false;
  }

  uint32_t pick = (uint32_t)random((long)totalDirScore) + 1;
  uint32_t sum  = 0;
  uint8_t  chosenDir = validDirs[dirCount - 1];
  for (uint8_t i = 0; i < dirCount; i++) { sum += dirScores[i]; if (pick <= sum) { chosenDir = validDirs[i]; break; } }

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
      totalFileScore       += (uint32_t)fileEntry.score;
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
  for (uint8_t i = 0; i < fileCount; i++) { sum += fileScores[i]; if (filePick <= sum) { chosenFile = validFiles[i]; break; } }

  if (!sdMgr.readFileEntry(chosenDir, chosenFile, &fileEntry)) return false;

  const uint32_t rawDuration = (uint32_t)fileEntry.size_kb * 1024UL / BYTES_PER_MS;
  if (rawDuration <= HEADER_MS + 100) {
    PF("[FragSelect] Too short: dir %03u file %03u\n", chosenDir, chosenFile);
    return false;
  }

  outFrag.dirIndex    = chosenDir;
  outFrag.fileIndex   = chosenFile;
  outFrag.startMs     = HEADER_MS;
  outFrag.durationMs  = rawDuration - HEADER_MS;
  outFrag.fadeMs      = 5000; // teruggezet zoals gevraagd

  return true;
}
