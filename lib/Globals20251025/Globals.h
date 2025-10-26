
#include <Arduino.h>

#pragma once
#include "macros.inc"
#include "HWconfig.h"
#include <atomic>
#include <type_traits>

#if __cplusplus >= 201703L
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable_v<T>
#else
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable<T>::value
#endif

template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    ptr->store(value, std::memory_order_relaxed);
}

template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    return ptr->load(std::memory_order_relaxed);
}


// === Existing Globals ===
// Aannames:
// - Alle MP3-bestanden zijn mono, 128 kbps
// - 128 kbps = 16000 bytes/sec → 1 ms ≈ 16 bytes
// - Dus bestandsgrootte gedeeld door 16 ≈ afspeelduur in ms
// === Instellingen ===

#define LOOPCYCLE 10
#define SECONDS_TICK 1000
#define BYTES_PER_MS 16
#define HEADER_MS 160
#define SD_MAX_DIRS 20
#define SD_MAX_FILES_PER_SUBDIR 254
#define WORDS_SUBDIR_ID 0
#define ROOT_DIRS "/.root_dirs"
#define FILES_DIR "/.files_dir"
#define SD_VERSION_FILENAME "/version.txt"
#define SD_VERSION "V2.00"
#define XSTR(x) STR(x)
#define STR(x) #x
#define SD_INDEX_VERSION  \
"SDMANAGER " SD_VERSION "\n" \
"Index format V3\n" \
"All MP3 files: mono, 128 kbps (max)\n" \
"headless files only (no ID3 tags)\n" \
"Bitrate: 128 kbps = 16,000 bytes/sec (1 ms ≈ " XSTR(BYTES_PER_MS) " bytes)\n" \
"Estimate ms = (file size / " XSTR(BYTES_PER_MS) ") - " XSTR(HEADER_MS) " [header]\n" \
"Folders: /NNN/   (NNN=0.." XSTR(SD_MAX_DIRS) ", max " XSTR(SD_MAX_DIRS) ")\n" \
"Files:   MMM.mp3 (MMM=1.." XSTR(SD_MAX_FILES_PER_SUBDIR) ", max " XSTR(SD_MAX_FILES_PER_SUBDIR) " per folder)\n" \
"Special: /000/ = reserved/skip\n"
#define MAX_LOOP_SILENCE 5000// max 5 sec blocking
#define SDPATHLENGTH 32
// SD card
#define SD_PROTECT_BEGIN()    \
    if (isSDBusy()) { PF("[SD_PROTECT] Busy, skip!\n"); return false; } \
    setSDBusy(true);
#define SD_PROTECT_END()      \
    setSDBusy(false);
#define SD_PROTECT_BEGIN_V()  \
    if (isSDBusy()) { PF("[SD_PROTECT] Busy, skip!\n"); return; } \
    setSDBusy(true);
void setSDReady(bool ready);
bool isSDReady();
void setSDBusy(bool v) ;
bool isSDBusy()   ;
// Temp + Voltage
float getTemp();
void setTemp(float value);
float getVoltage();
void setVoltage(float value);
// Counter
uint32_t getCount();
void setCount(uint32_t value);
// Flags
bool getFlag();
void setFlag(bool value);

// Sensor
float getLux();
void setLux(float value);
float getSensorValue();
void setSensorValue(float value);
//bool getWiFiConnected();
bool isWiFiConnected();
void setWiFiConnected(bool value);
//ledje
// extern bool isLEDon(void);
extern void setLedStatus(bool value);
// === Light Brightness Control ===
float getWebBrightness();
void setWebBrightness(float value);
bool isWebInterfaceActive();
void setWebInterfaceActive(bool value);
uint8_t getBaseBrightness();
void setBaseBrightness(uint8_t value);
// === Audio ===
float getAudioLevel();           // intern volume tijdens fragment
void setAudioLevel(float value);
float getWebAudioLevel();        // uit webinterface
void setWebAudioLevel(float value);
float getTimeAudioLevel();       // afhankelijk van tijdzone (nacht omlaag)
void setTimeAudioLevel(float value);
void setAudioLevelRaw(int16_t value);
int16_t getAudioLevelRaw();

// BaseGain = MAX_AUDIO_VOLUME × context × web × optional (zonder fade)
float getBaseGain();
void  setBaseGain(float value);
// Combinatie
float getEffectiveAudioLevel();  // audioLevel × web × time
// === Audio busy flags ===
bool isAudioBusy();
void setAudioBusy(bool value);
bool  getCurrentDirFile(uint8_t& dir, uint8_t& file);
void  setCurrentDirFile(uint8_t dir, uint8_t file);
void resetAudioState();
bool isFragmentPlaying();
void setFragmentPlaying(bool v);
bool isSentencePlaying();
void setSentencePlaying(bool v);
// === Word timing ===
// unsigned long getWordStartMillis();
// void setWordStartMillis(unsigned long ms);
// unsigned long getWordDurationMs();
// void setWordDurationMs(unsigned long ms);
bool isTimeFetched();
void setTimeFetched(bool value);
void showTimeDate(void);
void bootRandomSeed();
// fading
#define MAX_FADE_STEPS 15
void setFadeValue(uint8_t index, float value);
float getFadeValue(uint8_t index);
void setFFade(float value);
float getFFade();
//tts
String urlencode(const String& s) ;
extern bool ttsActive;
extern bool wordPlaying;
extern int  currentWordID;

// --- Utility functions ---
String twoDigits(uint8_t v);
const char *weekdayName(uint8_t dow);
const char *monthName(uint8_t month);
