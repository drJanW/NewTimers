#include "ContextFlags.h"
#include "ShiftEnums.h"
#include "TimeOfDay.h"
#include "ContextManager.h"
#include "Globals.h"

namespace ContextFlags {

// ============================================================
// Season detection (based on month, Northern Hemisphere)
// ============================================================
uint64_t getSeasonBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextManager::time();
    uint8_t month = ctx.month;
    
    // Spring: March (3), April (4), May (5)
    if (month >= 3 && month <= 5) {
        bits |= (1ULL << STATUS_SPRING);
    }
    // Summer: June (6), July (7), August (8)
    else if (month >= 6 && month <= 8) {
        bits |= (1ULL << STATUS_SUMMER);
    }
    // Autumn: September (9), October (10), November (11)
    else if (month >= 9 && month <= 11) {
        bits |= (1ULL << STATUS_AUTUMN);
    }
    // Winter: December (12), January (1), February (2)
    else {
        bits |= (1ULL << STATUS_WINTER);
    }
    
    return bits;
}

// ============================================================
// Weekday detection (from dayOfWeek: 0=Sunday, 1=Monday, etc.)
// ============================================================
uint64_t getWeekdayBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextManager::time();
    uint8_t dow = ctx.dayOfWeek;  // 0=Sun, 1=Mon, 2=Tue, ... 6=Sat
    
    // Map dayOfWeek to individual day flags
    switch (dow) {
        case 0: bits |= (1ULL << STATUS_SUNDAY);    break;
        case 1: bits |= (1ULL << STATUS_MONDAY);    break;
        case 2: bits |= (1ULL << STATUS_TUESDAY);   break;
        case 3: bits |= (1ULL << STATUS_WEDNESDAY); break;
        case 4: bits |= (1ULL << STATUS_THURSDAY);  break;
        case 5: bits |= (1ULL << STATUS_FRIDAY);    break;
        case 6: bits |= (1ULL << STATUS_SATURDAY);  break;
    }
    
    // Weekend flag (Saturday or Sunday)
    if (dow == 0 || dow == 6) {
        bits |= (1ULL << STATUS_WEEKEND);
    }
    
    return bits;
}

// ============================================================
// Weather/temperature detection (from fetched outdoor temp)
// Uses average of min/max for "current" feel
// ============================================================
uint64_t getWeatherBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextManager::time();
    
    if (!ctx.hasWeather) {
        // No weather data yet - return no weather flags
        return bits;
    }
    
    // Use average of daily min/max as "ambient" temperature
    float avgTemp = (ctx.weatherMinC + ctx.weatherMaxC) / 2.0f;
    
    // Classify into temperature bands
    if (avgTemp < 0.0f) {
        bits |= (1ULL << STATUS_FREEZING);
    } else if (avgTemp < 10.0f) {
        bits |= (1ULL << STATUS_COLD);
    } else if (avgTemp < 20.0f) {
        bits |= (1ULL << STATUS_MILD);
    } else if (avgTemp < 30.0f) {
        bits |= (1ULL << STATUS_WARM);
    } else {
        bits |= (1ULL << STATUS_HOT);
    }
    
    return bits;
}

// ============================================================
// Moon phase detection (from moonPhase: 0=new, 0.5=full, 1=new)
// ============================================================
uint64_t getMoonPhaseBits() {
    uint64_t bits = 0;
    const auto& ctx = ContextManager::time();
    float phase = ctx.moonPhase;  // 0.0 to 1.0
    
    // Divide lunar cycle into 4 phases:
    // New Moon:   0.00 - 0.125 or 0.875 - 1.00 (dark moon)
    // Waxing:     0.125 - 0.375 (growing toward full)
    // Full Moon:  0.375 - 0.625 (bright moon)
    // Waning:     0.625 - 0.875 (shrinking toward new)
    
    if (phase < 0.125f || phase >= 0.875f) {
        bits |= (1ULL << STATUS_NEW_MOON);
    } else if (phase < 0.375f) {
        bits |= (1ULL << STATUS_WAXING);
    } else if (phase < 0.625f) {
        bits |= (1ULL << STATUS_FULL_MOON);
    } else {
        bits |= (1ULL << STATUS_WANING);
    }
    
    return bits;
}

// ============================================================
// Time-of-day (delegates to existing TimeOfDay namespace)
// ============================================================
uint64_t getTimeOfDayBits() {
    return TimeOfDay::getActiveStatusBits();
}

// ============================================================
// Combined: all context flags OR'd together
// ============================================================
uint64_t getFullContextBits() {
    uint64_t bits = 0;
    
    bits |= getTimeOfDayBits();
    bits |= getSeasonBits();
    bits |= getWeekdayBits();
    bits |= getWeatherBits();
    bits |= getMoonPhaseBits();
    
    return bits;
}

// ============================================================
// Debug helper: print active flags
// ============================================================
void printActiveFlags(uint64_t bits) {
    PL("[ContextFlags] Active:");
    
    // Time-of-day
    if (bits & (1ULL << STATUS_NIGHT))     PP(" Night");
    if (bits & (1ULL << STATUS_DAWN))      PP(" Dawn");
    if (bits & (1ULL << STATUS_MORNING))   PP(" Morning");
    if (bits & (1ULL << STATUS_LIGHT))     PP(" Light");
    if (bits & (1ULL << STATUS_DAY))       PP(" Day");
    if (bits & (1ULL << STATUS_AFTERNOON)) PP(" Afternoon");
    if (bits & (1ULL << STATUS_DUSK))      PP(" Dusk");
    if (bits & (1ULL << STATUS_EVENING))   PP(" Evening");
    if (bits & (1ULL << STATUS_DARK))      PP(" Dark");
    if (bits & (1ULL << STATUS_AM))        PP(" AM");
    if (bits & (1ULL << STATUS_PM))        PP(" PM");
    
    // Seasons
    if (bits & (1ULL << STATUS_SPRING))    PP(" Spring");
    if (bits & (1ULL << STATUS_SUMMER))    PP(" Summer");
    if (bits & (1ULL << STATUS_AUTUMN))    PP(" Autumn");
    if (bits & (1ULL << STATUS_WINTER))    PP(" Winter");
    
    // Weather
    if (bits & (1ULL << STATUS_FREEZING))  PP(" Freezing");
    if (bits & (1ULL << STATUS_COLD))      PP(" Cold");
    if (bits & (1ULL << STATUS_MILD))      PP(" Mild");
    if (bits & (1ULL << STATUS_WARM))      PP(" Warm");
    if (bits & (1ULL << STATUS_HOT))       PP(" Hot");
    
    // Weekdays
    if (bits & (1ULL << STATUS_MONDAY))    PP(" Monday");
    if (bits & (1ULL << STATUS_TUESDAY))   PP(" Tuesday");
    if (bits & (1ULL << STATUS_WEDNESDAY)) PP(" Wednesday");
    if (bits & (1ULL << STATUS_THURSDAY))  PP(" Thursday");
    if (bits & (1ULL << STATUS_FRIDAY))    PP(" Friday");
    if (bits & (1ULL << STATUS_SATURDAY))  PP(" Saturday");
    if (bits & (1ULL << STATUS_SUNDAY))    PP(" Sunday");
    if (bits & (1ULL << STATUS_WEEKEND))   PP(" Weekend");
    
    // Moon
    if (bits & (1ULL << STATUS_NEW_MOON))  PP(" NewMoon");
    if (bits & (1ULL << STATUS_WAXING))    PP(" Waxing");
    if (bits & (1ULL << STATUS_FULL_MOON)) PP(" FullMoon");
    if (bits & (1ULL << STATUS_WANING))    PP(" Waning");
    
    PL("");
}

} // namespace ContextFlags
