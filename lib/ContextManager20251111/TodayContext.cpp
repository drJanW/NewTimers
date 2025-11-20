#include "TodayContext.h"

#include "Globals.h"
#include "PRTClock.h"
#include "SdPathUtils.h"

namespace {

struct TodayContextLogLimiter {
    uint32_t lastNoCalendar{0};
    uint32_t lastThemeFallback{0};
    uint32_t lastThemeUnavailable{0};
    uint32_t lastPatternFallback{0};
    uint32_t lastPatternUnavailable{0};
    uint32_t lastColorFallback{0};
    uint32_t lastColorUnavailable{0};

    static uint32_t makeKey(uint16_t year, uint8_t month, uint8_t day) {
        return (static_cast<uint32_t>(year) << 16) |
               (static_cast<uint32_t>(month) << 8) |
               static_cast<uint32_t>(day);
    }

    static bool shouldLog(uint32_t &slot, uint16_t year, uint8_t month, uint8_t day) {
        const uint32_t key = makeKey(year, month, day);
        if (slot == key) {
            return false;
        }
        slot = key;
        return true;
    }

    bool logNoCalendar(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastNoCalendar, year, month, day);
    }

    bool logThemeFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastThemeFallback, year, month, day);
    }

    bool logThemeUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastThemeUnavailable, year, month, day);
    }

    bool logPatternFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastPatternFallback, year, month, day);
    }

    bool logPatternUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastPatternUnavailable, year, month, day);
    }

    bool logColorFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastColorFallback, year, month, day);
    }

    bool logColorUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastColorUnavailable, year, month, day);
    }
};

TodayContextLogLimiter g_logLimiter;

enum class RepositoryLogState : uint8_t {
    Unknown,
    Ready,
    NotReady
};

struct RepositoryInitLogState {
    bool invalidRoot = false;
    bool calendarInitFailed = false;
    bool themeBoxInitFailed = false;
    bool patternInitFailed = false;
    bool colorInitFailed = false;
};

RepositoryLogState g_repoLogState = RepositoryLogState::Unknown;
RepositoryInitLogState g_repoInitLogs;

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
    String root_{"/"};
    bool ready_{false};
};

ContextRepository g_repo;

bool ContextRepository::init(fs::FS& sd, const char* rootPath) {
    ready_ = false;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = SdPathUtils::sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        if (!g_repoInitLogs.invalidRoot) {
            PF("[TodayContext] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
            g_repoInitLogs.invalidRoot = true;
        }
        root_ = "/";
    } else {
        g_repoInitLogs.invalidRoot = false;
        root_ = sanitized;
    }

    const char* rootCStr = root_.c_str();

    if (!calendar_.begin(sd, rootCStr)) {
        if (!g_repoInitLogs.calendarInitFailed) {
            PF("[TodayContext] CalendarManager init failed\n");
            g_repoInitLogs.calendarInitFailed = true;
        }
        return false;
    }
    g_repoInitLogs.calendarInitFailed = false;
    if (!themeBoxes_.begin(sd, rootCStr)) {
        if (!g_repoInitLogs.themeBoxInitFailed) {
            PF("[TodayContext] ThemeBoxManager init failed\n");
            g_repoInitLogs.themeBoxInitFailed = true;
        }
        return false;
    }
    g_repoInitLogs.themeBoxInitFailed = false;
    if (!patterns_.begin(sd, rootCStr)) {
        if (!g_repoInitLogs.patternInitFailed) {
            PF("[TodayContext] LightPatternStore init failed\n");
            g_repoInitLogs.patternInitFailed = true;
        }
        return false;
    }
    g_repoInitLogs.patternInitFailed = false;
    if (!colors_.begin(sd, rootCStr)) {
        if (!g_repoInitLogs.colorInitFailed) {
            PF("[TodayContext] LightColorStore init failed\n");
            g_repoInitLogs.colorInitFailed = true;
        }
        return false;
    }
    g_repoInitLogs.colorInitFailed = false;

    ready_ = true;
    if (g_repoLogState != RepositoryLogState::Ready) {
        PF("[TodayContext] Repository initialised\n");
        g_repoLogState = RepositoryLogState::Ready;
    }
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
        if (g_repoLogState != RepositoryLogState::NotReady) {
            PF("[TodayContext] Repository not ready\n");
            g_repoLogState = RepositoryLogState::NotReady;
        }
        return false;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    if (!resolveDate(year, month, day)) {
        return false;
    }

    CalendarEntry entry;
    const bool hasCalendarEntry = calendar_.findEntry(year, month, day, entry);
    if (!hasCalendarEntry) {
        entry.valid = false;
        entry.year = year;
        entry.month = month;
        entry.day = day;
        if (g_logLimiter.logNoCalendar(year, month, day)) {
            PF("[TodayContext] No calendar entry for %04u-%02u-%02u, using defaults\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
    }

    const ThemeBox* theme = nullptr;
    if (hasCalendarEntry && entry.themeBoxId != 0) {
        theme = themeBoxes_.find(entry.themeBoxId);
    }
    if (!theme) {
        const ThemeBox* fallbackTheme = themeBoxes_.active();
        if (fallbackTheme) {
            if (entry.themeBoxId != 0 && g_logLimiter.logThemeFallback(year, month, day)) {
                PF("[TodayContext] Theme box %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.themeBoxId),
                   static_cast<unsigned>(fallbackTheme->id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            theme = fallbackTheme;
        }
    }
    if (!theme) {
        if (g_logLimiter.logThemeUnavailable(year, month, day)) {
            PF("[TodayContext] No theme boxes available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
        return false;
    }

    const LightPattern* pattern = nullptr;
    if (hasCalendarEntry && entry.patternId != 0) {
        pattern = patterns_.find(entry.patternId);
    }
    if (!pattern) {
        const LightPattern* fallbackPattern = patterns_.active();
        if (fallbackPattern) {
            if (entry.patternId != 0 && g_logLimiter.logPatternFallback(year, month, day)) {
                PF("[TodayContext] Pattern %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.patternId),
                   static_cast<unsigned>(fallbackPattern->id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            pattern = fallbackPattern;
        }
    }
    if (!pattern) {
        if (g_logLimiter.logPatternUnavailable(year, month, day)) {
            PF("[TodayContext] No light patterns available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
        return false;
    }

    const LightColor* color = nullptr;
    if (hasCalendarEntry && entry.colorId != 0) {
        color = colors_.find(entry.colorId);
    }
    if (!color) {
        const LightColor* fallbackColor = colors_.active();
        if (fallbackColor) {
            if (entry.colorId != 0 && g_logLimiter.logColorFallback(year, month, day)) {
                PF("[TodayContext] Color %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.colorId),
                   static_cast<unsigned>(fallbackColor->id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            color = fallbackColor;
        }
    }
    if (!color) {
        if (g_logLimiter.logColorUnavailable(year, month, day)) {
            PF("[TodayContext] No light colors available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
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
