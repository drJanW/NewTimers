#include "LightManager.h"
#include <FastLED.h>
#include "AudioState.h"
#include "SensorManager.h"
#include "TimerManager.h"
#include <math.h>
#include <atomic>

namespace {

std::atomic<float> s_webBrightness{1.0f};
std::atomic<bool>  s_webInterfaceActive{false};
std::atomic<uint8_t> s_baseBrightness{128};

} // namespace

int iv_updateLightManager = LOOPCYCLE;

float getWebBrightness() {
  float value = s_webBrightness.load(std::memory_order_relaxed);
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  return value;
}

void setWebBrightness(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  s_webBrightness.store(value, std::memory_order_relaxed);
}

bool isWebInterfaceActive() {
  return s_webInterfaceActive.load(std::memory_order_relaxed);
}

void setWebInterfaceActive(bool value) {
  s_webInterfaceActive.store(value, std::memory_order_relaxed);
}

uint8_t getBaseBrightness() {
  return s_baseBrightness.load(std::memory_order_relaxed);
}

void setBaseBrightness(uint8_t value) {
  s_baseBrightness.store(value, std::memory_order_relaxed);
}

void cb_updateLightManager() { updateLightManager(); }

// === LED buffer ===
CRGB leds[NUM_LEDS];

// === State & Animatie voor CircleShow ===
static LightShowParams showParams;
static CRGB palette[PALETTE_SIZE];

static uint8_t xPhase = 0, yPhase = 0;
static uint8_t xCycleSec = 10, yCycleSec = 10;

static uint8_t colorPhase = 0;
static uint8_t brightPhase = 0;

static uint8_t colorCycleSec = 10;
static uint8_t brightCycleSec = 10;

// === Render-cadans (50 ms) ===
static volatile bool showDue = false;
static void showTimerCallback() { showDue = true; }

// === Timer callbacks ===
static void colorCycleCallback() { colorPhase++; }
static void brightCycleCallback() { brightPhase++; }
static void xPhaseCallback() { xPhase++; }
static void yPhaseCallback() { yPhase++; }

// === Timers helpers ===
static void startColorCycleTimer(uint8_t sec) {
  auto &tm = TimerManager::instance();
  uint32_t interval = (sec * 1000UL) / 255UL;
  if (interval == 0) interval = 1;
  if (!tm.restart(interval, 0, colorCycleCallback)) {
    PF("[LightManager] Failed to start color cycle timer\n");
  }
}

static void startBrightCycleTimer(uint8_t sec) {
  auto &tm = TimerManager::instance();
  uint32_t interval = (sec * 1000UL) / 255UL;
  if (interval == 0) interval = 1;
  if (!tm.restart(interval, 0, brightCycleCallback)) {
    PF("[LightManager] Failed to start brightness cycle timer\n");
  }
}

static void startXPhaseTimer(uint8_t sec) {
  uint32_t interval = (sec * 1000UL) / 255UL;
  if (interval == 0) interval = 1;
  auto &tm = TimerManager::instance();
  if (!tm.restart(interval, 0, xPhaseCallback)) {
    PF("[LightManager] Failed to start X phase timer\n");
  }
}

static void startYPhaseTimer(uint8_t sec) {
  uint32_t interval = (sec * 1000UL) / 255UL;
  if (interval == 0) interval = 1;
  auto &tm = TimerManager::instance();
  if (!tm.restart(interval, 0, yPhaseCallback)) {
    PF("[LightManager] Failed to start Y phase timer\n");
  }
}

static void startUpdateTimer(uint32_t intervalMs) {
  if (intervalMs == 0) intervalMs = 1;
  auto &tm = TimerManager::instance();
  if (!tm.restart(intervalMs, 0, cb_updateLightManager)) {
    PF("[LightManager] Failed to start update timer\n");
  }
}

// === Getters (optioneel extern gebruikt) ===
uint8_t getColorPhase()  { return colorPhase; }
uint8_t getBrightPhase() { return brightPhase; }

// === INIT/SETUP ===
void bootLightManager() {
  PF("[LightManager] init start\n");
  FastLED.addLeds<LED_TYPE, PIN_RGB, LED_RGB_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(MAX_VOLTS, MAX_MILLIAMPS);
  FastLED.setBrightness(MAX_BRIGHTNESS);

  if (!loadLEDMapFromSD("/ledmap.bin")) {
    PF("[LightManager] LED map fallback active\n");
  }

  // render-cadans 50 ms
  auto &tm = TimerManager::instance();
  if (!tm.restart(50, 0, showTimerCallback)) {
    PF("[LightManager] Failed to start show timer\n");
  }
  showDue = true;

  // update dispatcher (exact één keer)
  startUpdateTimer(iv_updateLightManager);

  startColorCycleTimer(colorCycleSec);
  startBrightCycleTimer(brightCycleSec);
  PF("[LightManager] init done\n");
}

// === Dispatcher/Update ===
void updateLightManager() {
  if (!showDue) return;
  showDue = false;

  updateDynamicBrightness();

  uint8_t cPh = getColorPhase();
  uint8_t bPh = getBrightPhase();

  float baseRadius = showParams.radius;
  float radiusOsc  = showParams.radiusOsc;

  float animRadius = baseRadius;
  if (radiusOsc != 0.0f) {
    float osc = bPh / 255.0f;
    if (radiusOsc > 0.0f) {
      animRadius += fabsf(radiusOsc) * sinf(osc * 2.0f * M_PI * showParams.gradientSpeed);
    } else {
      animRadius = -showParams.fadeWidth + fabsf(radiusOsc) * osc;
    }
  }

  float centerX = showParams.centerX, centerY = showParams.centerY;
  if (showParams.xAmp != 0.0f) {
    float px = xPhase / 255.0f;
    centerX += showParams.xAmp * sinf(px * 2.0f * M_PI);
  }
  if (showParams.yAmp != 0.0f) {
    float py = yPhase / 255.0f;
    centerY += showParams.yAmp * sinf(py * 2.0f * M_PI);
  }

  generateRGBPalette(showParams.RGB1, showParams.RGB2, palette, PALETTE_SIZE);

  int windowWidth       = showParams.windowWidth > 0 ? showParams.windowWidth : 16;
  uint8_t vensterStart  = cPh;
  uint8_t maxBrightness = getBaseBrightness();

  for (int i = 0; i < NUM_LEDS; ++i) {
    LEDPos pos = getLEDPos(i);
    float dx = pos.x - centerX;
    float dy = pos.y - centerY;
    float dist = sqrtf(dx * dx + dy * dy);

    float blend = fabsf(dist - animRadius) / showParams.fadeWidth;
    if (blend > 1.0f) blend = 1.0f;
    if (blend < 0.0f) blend = 0.0f;

    float fade = 1.0f - blend;
    fade = fade * fade;

    int palIdx = (vensterStart + int(blend * (windowWidth - 1))) % PALETTE_SIZE;
    if (palIdx < 0) palIdx += PALETTE_SIZE;

    CRGB color = palette[palIdx];

    uint8_t brightness = showParams.minBrightness +
                         uint8_t(fade * (maxBrightness - showParams.minBrightness));
    if (brightness > 0) color.nscale8_video(brightness);
    else                color = CRGB::Black;

    leds[i] = color;
  }

  FastLED.show();
}

// === Universele dispatcher ===
void PlayLightShow(const LightShowParams &p) {
  showParams = p;
  xCycleSec = p.xCycleSec > 0 ? p.xCycleSec : 10;
  yCycleSec = p.yCycleSec > 0 ? p.yCycleSec : 10;

  startColorCycleTimer(p.colorCycleSec);
  startBrightCycleTimer(p.brightCycleSec);
  startXPhaseTimer(xCycleSec);
  startYPhaseTimer(yCycleSec);
}

// === Brightness helpers ===
void updateDynamicBrightness() {
  float wb = isWebInterfaceActive() ? getWebBrightness() : 1.0f;

  float factor = wb;
  if (isAudioBusy()) {
    uint16_t raw = getAudioLevelRaw();
    if (raw) {
      float n = raw / 32768.0f;
      float adj = sqrtf(n) * 1.2f;
      if (adj > 1.0f) adj = 1.0f;
      factor = adj * wb;
    }
  }

  uint8_t base = getBaseBrightness();
  uint8_t br   = (uint8_t)(factor * base);

  if (base && br < BRIGHTNESS_FLOOR && getWebBrightness() > 0.0f) br = BRIGHTNESS_FLOOR;

  FastLED.setBrightness(br);
}

void updateBaseBrightness() {
  float luxFactor = 1.0f - SensorManager::ambientLux();
  float webFactor = getWebBrightness();
  float total = luxFactor * webFactor;
  if (total < 0.0f) total = 0.0f;
  if (total > 1.0f) total = 1.0f;

  uint8_t base = (uint8_t)(MAX_BRIGHTNESS * total);
  setBaseBrightness(base);
}

// === RGB/HULP ===
void generateRGBPalette(const CRGB &colorA, const CRGB &colorB, CRGB *pal, int n) {
  for (int i = 0; i < n; ++i) {
    float t = (float)i / (float)(n - 1);
    uint8_t blend;
    if (t < 0.5f) {
      blend = (uint8_t)(t * 2.0f * 255.0f);
      pal[i] = CRGB(
        lerp8by8(colorA.r, colorB.r, blend),
        lerp8by8(colorA.g, colorB.g, blend),
        lerp8by8(colorA.b, colorB.b, blend));
    } else {
      blend = (uint8_t)((1.0f - (t - 0.5f) * 2.0f) * 255.0f);
      pal[i] = CRGB(
        lerp8by8(colorA.r, colorB.r, blend),
        lerp8by8(colorA.g, colorB.g, blend),
        lerp8by8(colorA.b, colorB.b, blend));
    }
  }
}

CRGB blendRGB(const CRGB &c1, const CRGB &c2, uint8_t blend) {
  return CRGB(
    lerp8by8(c1.r, c2.r, blend),
    lerp8by8(c1.g, c2.g, blend),
    lerp8by8(c1.b, c2.b, blend));
}

void nextImmediate() {
  auto &tm = TimerManager::instance();
  tm.cancel(colorCycleCallback);
  tm.cancel(brightCycleCallback);
  tm.cancel(xPhaseCallback);
  tm.cancel(yPhaseCallback);

  startColorCycleTimer(10);
  startBrightCycleTimer(10);
  startXPhaseTimer(xCycleSec);
  startYPhaseTimer(yCycleSec);

  showDue = true;
  updateLightManager();
}

// === LightManager singleton facade ===
LightManager& LightManager::instance() {
  static LightManager inst;
  return inst;
}

void LightManager::begin() {
  PF("[LightManager] begin()\n");
  bootLightManager();
  PF("[LightManager] begin() complete\n");
}

void LightManager::update() {
  updateLightManager();
}

void LightManager::setBrightness(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > MAX_BRIGHTNESS) value = MAX_BRIGHTNESS;
  uint8_t target = static_cast<uint8_t>(value + 0.5f);
  setBaseBrightness(target);
  updateDynamicBrightness();
  showDue = true;
}

float LightManager::getBrightness() const {
  return static_cast<float>(getBaseBrightness());
}

void LightManager::capBrightness(float maxValue) {
  if (maxValue < 0.0f) maxValue = 0.0f;
  if (maxValue > MAX_BRIGHTNESS) maxValue = MAX_BRIGHTNESS;
  uint8_t cap = static_cast<uint8_t>(maxValue + 0.5f);
  uint8_t current = getBaseBrightness();
  if (current > cap) {
    setBaseBrightness(cap);
    updateDynamicBrightness();
    showDue = true;
  }
}

void LightManager::showOtaPattern() {
  // Simple OTA indicator: solid orange with current base brightness
  fill_solid(leds, NUM_LEDS, CRGB::OrangeRed);
  FastLED.setBrightness(getBaseBrightness());
  FastLED.show();
  showDue = true;
}

