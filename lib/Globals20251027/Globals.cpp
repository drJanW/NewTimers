#include "Arduino.h"
// Globals.cpp
// Strict per-variable layout. No legacy code, no double locking, no canvas.
#include "Globals.h"
//#include "fetchManager.h"  //todo!!!!
#include <math.h>
#include <driver/adc.h>
//#include "adc_port.h"
#include <esp_system.h>   // esp_random()

// --- Temp ---
static std::atomic<float> _valTemp{0.0f};
void  setTemp(float v) { setMux(v, &_valTemp); }
float getTemp()        { return getMux(&_valTemp); }
// --- Voltage ---
static std::atomic<float> _valVoltage{0.0f};
void  setVoltage(float v) { setMux(v, &_valVoltage); }
float getVoltage()        { return getMux(&_valVoltage); }
// --- Flag ---
static std::atomic<bool> _valFlag{false};
void  setFlag(bool v)  { setMux(v, &_valFlag); }
bool  getFlag()        { return getMux(&_valFlag); }

// --- Lux ---
static std::atomic<float> _valLux{0.0f};
void  setLux(float v) { setMux(v, &_valLux); }
float getLux()        { return getMux(&_valLux); }
// --- SensorValue ---
static std::atomic<float> _valSensorValue{0.0f};
void  setSensorValue(float v) { setMux(v, &_valSensorValue); }
float getSensorValue()        { return getMux(&_valSensorValue); }
// --- WebBrightness ---
static std::atomic<float> _valWebBrightness{1.0f};
void  setWebBrightness(float v) { setMux(v, &_valWebBrightness); }
float getWebBrightness()        { return getMux(&_valWebBrightness); }
// --- WebInterfaceActive ---
static std::atomic<bool> _valWebInterfaceActive{false};
void setWebInterfaceActive(bool v) { setMux(v, &_valWebInterfaceActive); }
bool isWebInterfaceActive()        { return getMux(&_valWebInterfaceActive); }
// --- BaseBrightness ---
static std::atomic<uint8_t> _valBaseBrightness{128};
void     setBaseBrightness(uint8_t v) { setMux(v, &_valBaseBrightness); }
uint8_t  getBaseBrightness()          { return getMux(&_valBaseBrightness); }
// --- AudioLevel ---
static std::atomic<float> _valAudioLevel{0.0f};
void  setAudioLevel(float v) { setMux(v, &_valAudioLevel); }
float getAudioLevel()        { return getMux(&_valAudioLevel); }
static std::atomic<float> _baseGain{MAX_AUDIO_VOLUME};
void  setBaseGain(float v) { setMux(v, &_baseGain); }
float getBaseGain()        { return getMux(&_baseGain); }
// --- WebAudioLevel ---
static std::atomic<float> _valWebAudioLevel{1.0f};
void  setWebAudioLevel(float v) { setMux(v, &_valWebAudioLevel); }
float getWebAudioLevel()        { return getMux(&_valWebAudioLevel); }
// --- TimeAudioLevel ---
static std::atomic<float> _valTimeAudioLevel{1.0f};
void  setTimeAudioLevel(float v) { setMux(v, &_valTimeAudioLevel); }
float getTimeAudioLevel()        { return getMux(&_valTimeAudioLevel); }
// --- AudioLevelRaw ---
static std::atomic<int16_t> _audioLevelRaw{0};
void     setAudioLevelRaw(int16_t v) { setMux(v, &_audioLevelRaw); }
int16_t  getAudioLevelRaw()          { return getMux(&_audioLevelRaw); }
// --- EffectiveAudioLevel (no set, just calc) ---
float getEffectiveAudioLevel() {
    float a = _valAudioLevel.load(std::memory_order_relaxed);
    float w = _valWebAudioLevel.load(std::memory_order_relaxed);
    float t = _valTimeAudioLevel.load(std::memory_order_relaxed);
    return a * w * t;
}
//################################################################################################
// --- AudioBusy ---
static std::atomic<bool> _audioBusy{false};
void setAudioBusy(bool value) { setMux(value, &_audioBusy); }
bool isAudioBusy()            { return getMux(&_audioBusy); }
static std::atomic<uint8_t> _curDir{0};
static std::atomic<uint8_t> _curFile{0};
static std::atomic<bool>    _curValid{false};

bool getCurrentDirFile(uint8_t& dir, uint8_t& file) {
  if (!getMux(&_curValid)) return false;
  dir  = getMux(&_curDir);
  file = getMux(&_curFile);
  return true;
}
void setCurrentDirFile(uint8_t dir, uint8_t file) {
  setMux<uint8_t>(dir,  &_curDir);
  setMux<uint8_t>(file, &_curFile);
  setMux<bool>(true,    &_curValid);
}

static std::atomic<bool> _FragmentPlaying{false};
bool isFragmentPlaying()            { return getMux(&_FragmentPlaying); }
void setFragmentPlaying(bool v)     { setMux(v, &_FragmentPlaying); }
static std::atomic<bool> _sentencePlaying{false};
bool isSentencePlaying()            { return getMux(&_sentencePlaying); }
void setSentencePlaying(bool v)     { setMux(v, &_sentencePlaying); }


// --- TimeFetched ---
static std::atomic<bool> _timeFetched{false};
void setTimeFetched(bool v) { setMux(v, &_timeFetched); }
bool isTimeFetched()        { return getMux(&_timeFetched); }
// --- WiFiConnected ---
static std::atomic<bool> _valWiFiConnected{false};
void setWiFiConnected(bool v) { setMux(v, &_valWiFiConnected); }
bool isWiFiConnected()        { return getMux(&_valWiFiConnected); }
// --- SDReady ---
static std::atomic<bool> _valSDReady{false};
void setSDReady(bool v) { setMux(v, &_valSDReady); }
bool isSDReady()        { return getMux(&_valSDReady); }
//=== sdBusy =======
static std::atomic<bool> _sdBusy{false};
void setSDBusy(bool value) {
    bool was = _sdBusy.load(std::memory_order_relaxed);
    if (was && value) {
        PF("[DEBUG][SDManager] WARNING: setSDBusy(true) called while already busy!\n");
    }
    if (!was && !value) {
        PF("[DEBUG][SDManager] WARNING: setSDBusy(false) called while already not busy!\n");
    }
    _sdBusy.store(value, std::memory_order_relaxed);
}
bool isSDBusy() { return getMux(&_sdBusy); }
// --- LEDStatus ---
static std::atomic<bool> _ledStatus{false};
void setLedStatus(bool v) { setMux(v, &_ledStatus); }
// bool isLEDon()            { return getMux(&_ledStatus); }
// --- PlayWord timing: StartMillis ---
// static std::atomic<unsigned long> _wordStartMillis{0};
// void           setWordStartMillis(unsigned long v) { setMux(v, &_wordStartMillis); }
// unsigned long  getWordStartMillis()                { return getMux(&_wordStartMillis); }
// --- PlayWord timing: DurationMs ---
// static std::atomic<unsigned long> _wordDurationMs{0};
// void           setWordDurationMs(unsigned long v) { setMux(v, &_wordDurationMs); }
// unsigned long  getWordDurationMs()                { return getMux(&_wordDurationMs); }
static float arrFades[MAX_FADE_STEPS];
static float fFadeValue = 0.0f;
portMUX_TYPE fadeMux = portMUX_INITIALIZER_UNLOCKED;
void setFadeValue(uint8_t index, float value) {
    if (index >= MAX_FADE_STEPS) return;
    portENTER_CRITICAL(&fadeMux);
    arrFades[index] = value;
    portEXIT_CRITICAL(&fadeMux);
}
float getFadeValue(uint8_t index) {
    if (index >= MAX_FADE_STEPS) return 0.0f;
    portENTER_CRITICAL(&fadeMux);
    float value = arrFades[index];
    portEXIT_CRITICAL(&fadeMux);
    return value;
}
void setFFade(float value) {
    portENTER_CRITICAL(&fadeMux);
    fFadeValue = value;
    portEXIT_CRITICAL(&fadeMux);
}
float getFFade() {
    portENTER_CRITICAL(&fadeMux);
    float value = fFadeValue;
    portEXIT_CRITICAL(&fadeMux);
    return value;
}

void bootRandomSeed() {
    uint32_t seed = esp_random() ^ micros();
    randomSeed(seed);
}

String urlencode(const String& s) {
    static const char* hex = "0123456789ABCDEF";
    String out; out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); ++i) {
        uint8_t c = (uint8_t)s[i];
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += char(c);
        } else if (c == ' ') {
            out += '+'; // x-www-form-urlencoded
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}


bool ttsActive = false;
bool wordPlaying = false;
int  currentWordID = 0;


// --- Utility functions ---
String twoDigits(uint8_t v) {
    if (v < 10) {
        String result("0");
        result += String(v);
        return result;
    }
    return String(v);
}

const char *weekdayName(uint8_t dow) {
    static const char *names[] = {
        "zondag", "maandag", "dinsdag",
        "woensdag", "donderdag", "vrijdag", "zaterdag"};
    return (dow < 7) ? names[dow] : "";
}

const char *monthName(uint8_t month) {
    static const char *names[] = {
        "januari", "februari", "maart", "april",
        "mei", "juni", "juli", "augustus",
        "september", "oktober", "november", "december"};
    return (month >= 1 && month <= 12) ? names[month - 1] : "";
}

