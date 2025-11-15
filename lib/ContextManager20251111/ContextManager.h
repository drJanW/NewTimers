#pragma once
#include <Arduino.h>

namespace ContextManager {
  enum class WebCmd : uint8_t { None = 0, NextTrack, DeleteFile };

  struct TimeContext {
    uint8_t hour{0};
    uint8_t minute{0};
    uint8_t second{0};
    uint16_t year{2000};
    uint8_t month{1};
    uint8_t day{1};
    uint8_t dayOfWeek{0};
    uint16_t dayOfYear{1};
    uint8_t sunriseHour{0};
    uint8_t sunriseMinute{0};
    uint8_t sunsetHour{0};
    uint8_t sunsetMinute{0};
    float moonPhase{0.0f};
    float weatherMinC{0.0f};
    float weatherMaxC{0.0f};
    bool hasWeather{false};
    bool synced{false};
  };

  const TimeContext& time();
  void refreshTimeSnapshot();
  void updateWeather(float minC, float maxC);
  void clearWeather();
}

// Webthread → post opdracht
void ContextManager_post(ContextManager::WebCmd cmd, uint8_t dir = 0, uint8_t file = 0);

// Start periodieke tick via TimerManager (NIET in loop() aanroepen)
void ContextManager_boot();
