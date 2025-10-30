#include "CalendarPolicy.h"

#include "Globals.h"
#include "../Audio/AudioPolicy.h"
#include "../Light/LightPolicy.h"
#include "../../SDManager20251024/SDManager.h"

#include <stdio.h>

namespace {

constexpr uint32_t kMinutesToMs = 60UL * 1000UL;
constexpr size_t   kMaxThemeDirs = 16;

String formatColor(uint32_t rgb) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%06lX", static_cast<unsigned long>(rgb & 0x00FFFFFFUL));
  return String(buf);
}

size_t parseThemeEntries(const String& entries, uint8_t* dirs, size_t maxCount) {
  if (!dirs || maxCount == 0) {
    return 0;
  }

  size_t count = 0;
  int start = 0;
  const int len = entries.length();

  while (start < len && count < maxCount) {
    int sep = entries.indexOf(',', start);
    String token = (sep >= 0) ? entries.substring(start, sep) : entries.substring(start);
    token.trim();
    if (!token.isEmpty()) {
      long value = token.toInt();
      if (value >= 0 && value <= 255) {
        dirs[count++] = static_cast<uint8_t>(value);
      }
    }
    if (sep < 0) {
      break;
    }
    start = sep + 1;
  }

  return count;
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
    AudioPolicy::clearThemeBox();
    return;
  }

  uint8_t dirs[kMaxThemeDirs];
  const size_t count = parseThemeEntries(box.entries, dirs, kMaxThemeDirs);
  if (count == 0) {
    PF("[CalendarPolicy] Theme box %s has no valid directories, clearing\n", box.id.c_str());
    AudioPolicy::clearThemeBox();
    return;
  }

  uint8_t filtered[kMaxThemeDirs];
  size_t filteredCount = 0;
  auto& sd = SDManager::instance();
  for (size_t i = 0; i < count; ++i) {
    DirEntry entry{};
    // Only keep directories that still have indexed fragment files.
    if (sd.readDirEntry(dirs[i], &entry) && entry.file_count > 0) {
      filtered[filteredCount++] = dirs[i];
    } else {
      PF("[CalendarPolicy] Theme dir %03u missing or empty, skipping\n", dirs[i]);
    }
  }

  if (filteredCount == 0) {
    PF("[CalendarPolicy] Theme box %s has no populated directories, clearing\n", box.id.c_str());
    AudioPolicy::clearThemeBox();
    return;
  }

  AudioPolicy::setThemeBox(filtered, filteredCount, box.id);
  PF("[CalendarPolicy] Theme box %s applied with %u directories\n",
     box.id.c_str(), static_cast<unsigned>(filteredCount));
}

void handleLightShow(const CalendarLightShow& show, const CalendarColorRange& colors) {
  if (!show.valid) {
    if (!colors.valid) {
      LightPolicy::clearCalendarLightshow();
      return;
    }
    PF("[CalendarPolicy] Light show request missing valid show, clearing\n");
    LightPolicy::clearCalendarLightshow();
    return;
  }

  LightPolicy::applyCalendarLightshow(show, colors);

  const uint32_t primary   = colors.valid ? colors.startColor : show.rgb1;
  const uint32_t secondary = colors.valid ? colors.endColor   : show.rgb2;

  PF("[CalendarPolicy] Light show %s applied (palette %s -> %s)\n",
     show.id.c_str(), formatColor(primary).c_str(), formatColor(secondary).c_str());
}

} // namespace CalendarPolicy
