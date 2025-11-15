#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include <FastLED.h>
#include "Globals.h"
#include "LEDMap.h"
#include "TimerManager.h"

class LightManager {
public:
  static LightManager& instance();

  void begin();
  void update();

  void setBrightness(float value);
  float getBrightness() const;
  void capBrightness(float maxValue);
  void showOtaPattern();

private:
  LightManager() = default;
  LightManager(const LightManager&) = delete;
  LightManager& operator=(const LightManager&) = delete;
};

  #define BRIGHTNESS_FLOOR 5 
// ===== ENUMS EN STRUCTS =====
// enum LightShow {    circleShow};

struct LightShowParams {
  CRGB RGB1, RGB2;
  uint8_t colorCycleSec, brightCycleSec, minBrightness, xCycleSec, yCycleSec;
  float fadeWidth, gradientSpeed, centerX, centerY, radius, radiusOsc, xAmp, yAmp;
  int   windowWidth;

   LightShowParams() = default;   // maak default ctor expliciet

  constexpr LightShowParams(
    CRGB a, CRGB b, int cCol, int cBrt, float fW, int minB, float gS,
    float cx, float cy, float r, int wW, float rOsc, float xA, float yA,
    int xC, int yC)
  : RGB1(a), RGB2(b),
    colorCycleSec(static_cast<uint8_t>(cCol)),
    brightCycleSec(static_cast<uint8_t>(cBrt)),
    fadeWidth(fW),
    minBrightness(static_cast<uint8_t>(minB)),
    gradientSpeed(gS),
    centerX(cx), centerY(cy), radius(r),
    windowWidth(wW),
    radiusOsc(rOsc), xAmp(xA), yAmp(yA),
    xCycleSec(static_cast<uint8_t>(xC)),
    yCycleSec(static_cast<uint8_t>(yC)) {}
};

/*
struct LightShowParams {
    //LightShow type = circleShow;
    CRGB RGB1 = CRGB::LightPink;
    CRGB RGB2 = CRGB::DeepPink;
    uint8_t colorCycleSec = 10;
    uint8_t brightCycleSec = 10;
    float fadeWidth = 8.0f;
    uint8_t minBrightness = 10;
    float gradientSpeed = 5.1f;
    float centerX     = 0.0f;
    float centerY     = 0.0f;
    float radius      = 20.0f;
    int   windowWidth = 16;
    float radiusOsc   = 0.0f;
    float xAmp        = 0.0f;
    float yAmp        = 0.0f;
    uint8_t xCycleSec = 10;
    uint8_t yCycleSec = 10;
};
*/
#define PALETTE_SIZE 256
extern CRGB leds[];

// ===== API =====
float getWebBrightness();
void setWebBrightness(float value);
bool isWebInterfaceActive();
void setWebInterfaceActive(bool value);
uint8_t getBaseBrightness();
void setBaseBrightness(uint8_t value);

void bootLightManager();
void updateLightManager();
void PlayLightShow(const LightShowParams&);

uint8_t getColorPhase();
uint8_t getBrightPhase();
void updateDynamicBrightness();
void updateBaseBrightness();
void generateRGBPalette(const CRGB& colorA, const CRGB& colorB, CRGB* palette, int n = PALETTE_SIZE);
CRGB blendRGB(const CRGB& c1, const CRGB& c2, uint8_t blend);

void nextImmediate();

#endif
