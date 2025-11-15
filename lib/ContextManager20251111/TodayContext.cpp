#include "TodayContext.h"

#include "Globals.h"
#include "PRTClock.h"

namespace {

class ContextRepository {
public:
    bool init(fs::FS& sd, const char* rootPath);
    bool ready() const { return ready_; }
    bool loadToday(TodayContext& ctx);

private:
    bool resolveDate(uint16_t& year, uint8_t& month, uint8_t& day) const;

    Context::CalendarManager calendar_;
    ThemeBoxManager themeBoxes_;
    LightPatternStore patterns_;
    LightColorStore colors_;
    bool ready_{false};
};

ContextRepository g_repo;

bool ContextRepository::init(fs::FS& sd, const char* rootPath) {
    ready_ = false;
    if (!calendar_.begin(sd, rootPath)) {
        PF("[TodayContext] CalendarManager init failed\n");
        return false;
    }
    if (!themeBoxes_.begin(sd, rootPath)) {
        PF("[TodayContext] ThemeBoxManager init failed\n");
        return false;
    }
    if (!patterns_.begin(sd, rootPath)) {
        PF("[TodayContext] LightPatternStore init failed\n");
        return false;
    }
    if (!colors_.begin(sd, rootPath)) {
        PF("[TodayContext] LightColorStore init failed\n");
        return false;
    }

    ready_ = true;
    PF("[TodayContext] Repository initialised\n");
    return true;
}

bool ContextRepository::resolveDate(uint16_t& year, uint8_t& month, uint8_t& day) const {
    auto& clock = PRTClock::instance();
    const uint16_t rawYear = clock.getYear();
    month = clock.getMonth();
    day = clock.getDay();
    if (rawYear == 0 || month == 0 || day == 0) {
        return false;
    }

    if (rawYear >= 1900) {
        year = rawYear;
    } else {
        year = static_cast<uint16_t>(2000 + rawYear);
    }
    return true;
}

bool ContextRepository::loadToday(TodayContext& ctx) {
    ctx = TodayContext{};
    if (!ready_) {
        PF("[TodayContext] Repository not ready\n");
        return false;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    if (!resolveDate(year, month, day)) {
        PF("[TodayContext] Clock not initialised\n");
        return false;
    }

    CalendarEntry entry;
    if (!calendar_.findEntry(year, month, day, entry)) {
        PF("[TodayContext] No calendar entry for %04u-%02u-%02u\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        return false;
    }

    const ThemeBox* theme = nullptr;
    if (!entry.themeBoxId.isEmpty()) {
        theme = themeBoxes_.find(entry.themeBoxId);
    }
    if (!theme) {
        const ThemeBox* fallbackTheme = themeBoxes_.active();
        if (fallbackTheme) {
            PF("[TodayContext] Theme box %s missing, falling back to %s for %04u-%02u-%02u\n",
               entry.themeBoxId.c_str(), fallbackTheme->id.c_str(), static_cast<unsigned>(year),
               static_cast<unsigned>(month), static_cast<unsigned>(day));
            theme = fallbackTheme;
        }
    }
    if (!theme) {
        PF("[TodayContext] No theme boxes available for %04u-%02u-%02u\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        return false;
    }

    const LightPattern* pattern = nullptr;
    if (!entry.patternId.isEmpty()) {
        pattern = patterns_.find(entry.patternId);
    }
    if (!pattern) {
        const LightPattern* fallbackPattern = patterns_.active();
        if (fallbackPattern) {
            PF("[TodayContext] Pattern %s missing, falling back to %s for %04u-%02u-%02u\n",
               entry.patternId.c_str(), fallbackPattern->id.c_str(), static_cast<unsigned>(year),
               static_cast<unsigned>(month), static_cast<unsigned>(day));
            pattern = fallbackPattern;
        }
    }
    if (!pattern) {
        PF("[TodayContext] No light patterns available for %04u-%02u-%02u\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        return false;
    }

    const LightColor* color = nullptr;
    if (!entry.colorId.isEmpty()) {
        color = colors_.find(entry.colorId);
    }
    if (!color) {
        const LightColor* fallbackColor = colors_.active();
        if (fallbackColor) {
            PF("[TodayContext] Color %s missing, falling back to %s for %04u-%02u-%02u\n",
               entry.colorId.c_str(), fallbackColor->id.c_str(), static_cast<unsigned>(year),
               static_cast<unsigned>(month), static_cast<unsigned>(day));
            color = fallbackColor;
        }
    }
    if (!color) {
        PF("[TodayContext] No light colors available for %04u-%02u-%02u\n",
           static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        return false;
    }

    ctx.valid = true;
    ctx.entry = entry;
    ctx.theme = *theme;
    ctx.pattern = *pattern;
    ctx.colors = *color;
    return true;
}

} // namespace

bool InitTodayContext(fs::FS& sd, const char* rootPath) {
    return g_repo.init(sd, rootPath);
}

bool TodayContextReady() {
    return g_repo.ready();
}

bool LoadTodayContext(TodayContext& ctx) {
    return g_repo.loadToday(ctx);
}
