#include "ContextManager.h"
#include "Globals.h"
#include "AudioState.h"
#include "TimerManager.h"
#include "AudioManager.h"
#include "PlayFragment.h"
#include "Audio/AudioDirector.h"
#include "SDVoting.h"
#include "PRTClock.h"

static volatile ContextManager::WebCmd s_cmd = ContextManager::WebCmd::None;
static volatile uint8_t s_cmdDir = 0, s_cmdFile = 0;
static bool s_nextPending = false;
static ContextManager::TimeContext s_timeContext{};
static float s_weatherMinC = 0.0f;
static float s_weatherMaxC = 0.0f;
static bool s_weatherValid = false;

namespace {

void updateTimeContext() {
  auto &clock = PRTClock::instance();
  s_timeContext.hour = clock.getHour();
  s_timeContext.minute = clock.getMinute();
  s_timeContext.second = clock.getSecond();
  s_timeContext.year = static_cast<uint16_t>(2000 + clock.getYear());
  s_timeContext.month = clock.getMonth();
  s_timeContext.day = clock.getDay();
  s_timeContext.dayOfWeek = clock.getDoW();
  s_timeContext.dayOfYear = clock.getDoY();
  s_timeContext.sunriseHour = clock.getSunriseHour();
  s_timeContext.sunriseMinute = clock.getSunriseMinute();
  s_timeContext.sunsetHour = clock.getSunsetHour();
  s_timeContext.sunsetMinute = clock.getSunsetMinute();
  s_timeContext.moonPhase = clock.getMoonPhaseValue();
  s_timeContext.synced = clock.isTimeFetched();

  if (s_weatherValid) {
    s_timeContext.hasWeather = true;
    s_timeContext.weatherMinC = s_weatherMinC;
    s_timeContext.weatherMaxC = s_weatherMaxC;
  } else {
    s_timeContext.hasWeather = false;
    s_timeContext.weatherMinC = 0.0f;
    s_timeContext.weatherMaxC = 0.0f;
  }
}

} // namespace

// 20 ms periodic via TimerManager
static void ctx_tick_cb() {
  updateTimeContext();

  // 1) pak één command
  ContextManager::WebCmd cmd = s_cmd;
  if (cmd != ContextManager::WebCmd::None) s_cmd = ContextManager::WebCmd::None;

  // 2) verwerk command
  if (cmd == ContextManager::WebCmd::NextTrack) {
    s_nextPending = true;
  } else if (cmd == ContextManager::WebCmd::DeleteFile) {
    if (!isAudioBusy() && !isSentencePlaying()) {
      SDVoting::deleteIndexedFile(s_cmdDir, s_cmdFile);
    } else {
      s_cmd = ContextManager::WebCmd::DeleteFile; // retry zodra idle
    }
  }

  // 3) NEXT uitvoeren
  if (s_nextPending) {
    if (isAudioBusy() || isSentencePlaying()) {
      AudioManager::instance().stop();
      return; // volgende tick start nieuwe
    }
    AudioFragment frag{};
    if (AudioDirector::selectRandomFragment(frag)) {
      if (!PlayAudioFragment::start(frag)) {
        PF("[ContextManager] NEXT failed: fragment start rejected\n");
      }
    }
    s_nextPending = false;
  }
}

void ContextManager_post(ContextManager::WebCmd cmd, uint8_t dir, uint8_t file) {
  s_cmdDir = dir; s_cmdFile = file; s_cmd = cmd;
}

void ContextManager_boot() {
  // Start een 20 ms heartbeat die de context events verwerkt.
  auto &timers = TimerManager::instance();
  timers.cancel(ctx_tick_cb);
  updateTimeContext();
  if (!timers.create(20, 0, ctx_tick_cb)) {
    PF("[ContextManager] Failed to start context tick timer\n");
  }
}

const ContextManager::TimeContext& ContextManager::time() {
  return s_timeContext;
}

void ContextManager::refreshTimeSnapshot() {
  updateTimeContext();
}

void ContextManager::updateWeather(float minC, float maxC) {
  s_weatherMinC = minC;
  s_weatherMaxC = maxC;
  s_weatherValid = true;
  updateTimeContext();
}

void ContextManager::clearWeather() {
  s_weatherValid = false;
  updateTimeContext();
}
