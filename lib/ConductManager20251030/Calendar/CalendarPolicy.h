#pragma once

#include <Arduino.h>

#include "Calendar.h"

namespace CalendarPolicy {

struct Decision {
  bool hasSentence{false};
  uint32_t sentenceIntervalMs{0};
  bool hasThemeBox{false};
};

void configure();
bool evaluate(const CalendarSnapshot& snapshot, Decision& decision);
void dispatchSentence(const String& phrase);
void handleThemeBox(const CalendarThemeBox& box);

} // namespace CalendarPolicy
