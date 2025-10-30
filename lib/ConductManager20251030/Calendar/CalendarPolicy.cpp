#include "CalendarPolicy.h"

#include "Globals.h"
#include "../Audio/AudioPolicy.h"

#include <stdio.h>

namespace {

constexpr uint32_t kMinutesToMs = 60UL * 1000UL;

String formatColor(uint32_t rgb) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%06lX", static_cast<unsigned long>(rgb & 0x00FFFFFFUL));
  return String(buf);
}

} // namespace

namespace CalendarPolicy {

void configure() {
  PF("[CalendarPolicy] configured\n");
}

bool evaluate(const CalendarSnapshot& snapshot, Decision& decision) {
  decision = Decision{};
  if (!snapshot.valid) {
    return false;
  }

  decision.hasSentence = snapshot.day.ttsSentence.length() > 0;
  if (decision.hasSentence) {
    decision.sentenceIntervalMs = static_cast<uint32_t>(snapshot.day.ttsIntervalMinutes) * kMinutesToMs;
  }

  decision.hasThemeBox = snapshot.theme.valid && snapshot.theme.entries.length() > 0;
  decision.hasLightShow = snapshot.light.valid;
  decision.hasColorRange = snapshot.color.valid;

  return true;
}

void dispatchSentence(const String& phrase) {
  if (phrase.isEmpty()) {
    return;
  }
  AudioPolicy::requestSentence(phrase);
}

void handleThemeBox(const CalendarThemeBox& box) {
  if (!box.valid) {
    return;
  }
  PF("[CalendarPolicy] Theme box %s -> entries %s\n",
     box.id.c_str(), box.entries.c_str());
}

void handleLightShow(const CalendarLightShow& show, const CalendarColorRange& colors) {
  if (!show.valid) {
    return;
  }

  const String startColor = colors.valid ? formatColor(colors.startColor) : String("#??????");
  const String endColor   = colors.valid ? formatColor(colors.endColor)   : String("#??????");

  PF("[CalendarPolicy] Light show %s pending (palette %s -> %s)\n",
     show.id.c_str(), startColor.c_str(), endColor.c_str());
}

} // namespace CalendarPolicy
