#include "ShiftStore.h"
#include "CsvUtils.h"
#include "SDManager.h"
#include "SDBusyGuard.h"
#include "Globals.h"
#include <SD.h>

namespace {
    constexpr const char* kColorShiftPath = "/colorsShifts.csv";
    constexpr const char* kPatternShiftPath = "/patternShifts.csv";
}

ShiftStore& ShiftStore::instance() {
    static ShiftStore inst;
    return inst;
}

bool ShiftStore::begin() {
    if (ready_) {
        return true;
    }
    
    bool colorOk = loadColorShiftsFromSD();
    bool patternOk = loadPatternShiftsFromSD();
    
    PF("[ShiftStore] Loaded %d color shifts, %d pattern shifts\n",
       colorShifts_.size(), patternShifts_.size());
    
    ready_ = true;  // Mark ready even if files missing (will just have no shifts)
    return colorOk || patternOk;
}

bool ShiftStore::parseStatusString(const String& s, uint8_t& out) {
    if (s == "isNight")     { out = STATUS_NIGHT; return true; }
    if (s == "isDawn")      { out = STATUS_DAWN; return true; }
    if (s == "isMorning")   { out = STATUS_MORNING; return true; }
    if (s == "isLight")     { out = STATUS_LIGHT; return true; }
    if (s == "isDay")       { out = STATUS_DAY; return true; }
    if (s == "isAfternoon") { out = STATUS_AFTERNOON; return true; }
    if (s == "isDusk")      { out = STATUS_DUSK; return true; }
    if (s == "isEvening")   { out = STATUS_EVENING; return true; }
    if (s == "isDark")      { out = STATUS_DARK; return true; }
    if (s == "isAM")        { out = STATUS_AM; return true; }
    if (s == "isPM")        { out = STATUS_PM; return true; }
    return false;
}

bool ShiftStore::parseColorParam(const String& s, uint8_t& out) {
    if (s == "colors.colorA.hue")        { out = COLOR_A_HUE; return true; }
    if (s == "colors.colorA.saturation") { out = COLOR_A_SAT; return true; }
    if (s == "colors.colorA.brightness") { out = COLOR_A_BRIGHT; return true; }
    if (s == "colors.colorA.value")      { out = COLOR_A_VALUE; return true; }
    if (s == "colors.colorB.hue")        { out = COLOR_B_HUE; return true; }
    if (s == "colors.colorB.saturation") { out = COLOR_B_SAT; return true; }
    if (s == "colors.colorB.brightness") { out = COLOR_B_BRIGHT; return true; }
    if (s == "colors.colorB.value")      { out = COLOR_B_VALUE; return true; }
    return false;
}

bool ShiftStore::parsePatternParam(const String& s, uint8_t& out) {
    if (s == "pattern.color_cycle_sec")  { out = PAT_COLOR_CYCLE; return true; }
    if (s == "pattern.bright_cycle_sec") { out = PAT_BRIGHT_CYCLE; return true; }
    if (s == "pattern.fade_width")       { out = PAT_FADE_WIDTH; return true; }
    if (s == "pattern.min_brightness")   { out = PAT_MIN_BRIGHT; return true; }
    if (s == "pattern.gradient_speed")   { out = PAT_GRADIENT_SPEED; return true; }
    if (s == "pattern.center_x")         { out = PAT_CENTER_X; return true; }
    if (s == "pattern.center_y")         { out = PAT_CENTER_Y; return true; }
    if (s == "pattern.radius")           { out = PAT_RADIUS; return true; }
    if (s == "pattern.window_width")     { out = PAT_WINDOW_WIDTH; return true; }
    if (s == "pattern.radius_osc")       { out = PAT_RADIUS_OSC; return true; }
    if (s == "pattern.x_amp")            { out = PAT_X_AMP; return true; }
    if (s == "pattern.y_amp")            { out = PAT_Y_AMP; return true; }
    if (s == "pattern.x_cycle_sec")      { out = PAT_X_CYCLE; return true; }
    if (s == "pattern.y_cycle_sec")      { out = PAT_Y_CYCLE; return true; }
    return false;
}

bool ShiftStore::loadColorShiftsFromSD() {
    if (!SDManager::isReady()) {
        PF("[ShiftStore] SD not ready for color shifts\n");
        return false;
    }
    
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    
    if (!SD.exists(kColorShiftPath)) {
        PF("[ShiftStore] %s not found\n", kColorShiftPath);
        return false;
    }
    
    File file = SD.open(kColorShiftPath, FILE_READ);
    if (!file) {
        return false;
    }
    
    colorShifts_.clear();
    
    String line;
    std::vector<String> columns;
    columns.reserve(4);
    bool headerSkipped = false;
    
    while (csv::readLine(file, line)) {
        if (line.isEmpty()) continue;
        
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;
        
        if (!headerSkipped) {
            headerSkipped = true;
            if (trimmed.startsWith("status")) continue;
        }
        
        csv::splitColumns(line, columns);
        if (columns.size() < 3) continue;
        
        float pct = columns[2].toFloat();
        if (pct == 0.0f) continue;  // SKIP ZEROS - no effect
        
        uint8_t statusId, paramId;
        if (!parseStatusString(columns[0], statusId)) continue;
        if (!parseColorParam(columns[1], paramId)) continue;
        
        ColorShiftEntry entry;
        entry.statusId = statusId;
        entry.paramId = paramId;
        entry.multiplier = 1.0f + (pct / 100.0f);  // -50% → 0.5, +100% → 2.0
        colorShifts_.push_back(entry);
    }
    
    file.close();
    return true;
}

bool ShiftStore::loadPatternShiftsFromSD() {
    if (!SDManager::isReady()) {
        PF("[ShiftStore] SD not ready for pattern shifts\n");
        return false;
    }
    
    SDBusyGuard guard;
    if (!guard.acquired()) {
        return false;
    }
    
    if (!SD.exists(kPatternShiftPath)) {
        PF("[ShiftStore] %s not found on SD\n", kPatternShiftPath);
        return false;
    }
    
    PF("[ShiftStore] Loading %s...\n", kPatternShiftPath);
    File file = SD.open(kPatternShiftPath, FILE_READ);
    if (!file) {
        return false;
    }
    
    patternShifts_.clear();
    
    String line;
    std::vector<String> columns;
    columns.reserve(4);
    bool headerSkipped = false;
    
    while (csv::readLine(file, line)) {
        if (line.isEmpty()) continue;
        
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty() || trimmed.charAt(0) == '#') continue;
        
        if (!headerSkipped) {
            headerSkipped = true;
            if (trimmed.startsWith("status")) continue;
        }
        
        csv::splitColumns(line, columns);
        if (columns.size() < 3) {
            PF("[ShiftStore] Pattern CSV: skip line, cols=%d\n", columns.size());
            continue;
        }
        
        float pct = columns[2].toFloat();
        if (pct == 0.0f) continue;  // SKIP ZEROS
        
        uint8_t statusId, paramId;
        if (!parseStatusString(columns[0], statusId)) {
            PF("[ShiftStore] Pattern CSV: unknown status '%s'\n", columns[0].c_str());
            continue;
        }
        if (!parsePatternParam(columns[1], paramId)) {
            PF("[ShiftStore] Pattern CSV: unknown param '%s'\n", columns[1].c_str());
            continue;
        }
        
        PatternShiftEntry entry;
        entry.statusId = statusId;
        entry.paramId = paramId;
        entry.multiplier = 1.0f + (pct / 100.0f);
        patternShifts_.push_back(entry);
    }
    
    file.close();
    return true;
}

void ShiftStore::computeColorMultipliers(uint64_t activeStatusBits, float* outMultipliers) const {
    // Initialize all to 1.0 (no change)
    for (int i = 0; i < COLOR_PARAM_COUNT; i++) {
        outMultipliers[i] = 1.0f;
    }
    
    // Multiply in all active shifts
    for (const auto& entry : colorShifts_) {
        if (activeStatusBits & (1ULL << entry.statusId)) {
            outMultipliers[entry.paramId] *= entry.multiplier;
        }
    }
}

void ShiftStore::computePatternMultipliers(uint64_t activeStatusBits, float* outMultipliers) const {
    // Initialize all to 1.0 (no change)
    for (int i = 0; i < PAT_PARAM_COUNT; i++) {
        outMultipliers[i] = 1.0f;
    }
    
    // Multiply in all active shifts
    for (const auto& entry : patternShifts_) {
        if (activeStatusBits & (1ULL << entry.statusId)) {
            outMultipliers[entry.paramId] *= entry.multiplier;
        }
    }
}
