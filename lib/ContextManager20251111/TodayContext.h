#pragma once

#include <Arduino.h>
#include <FS.h>

#include "CalendarManager.h"
#include "ThemeBoxManager.h"
#include "LightPatterns.h"
#include "LightColors.h"
#include "ContextModels.h"

bool InitTodayContext(fs::FS& sd, const char* rootPath = "/");
bool TodayContextReady();
bool LoadTodayContext(TodayContext& ctx);
