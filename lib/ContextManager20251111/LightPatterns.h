#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

class LightPatternStore {
public:
    bool begin(fs::FS& sd, const char* rootPath = "/");
    bool ready() const;
    const LightPattern* find(const String& id) const;
    const LightPattern* active() const;
    void clear();

private:
    bool load();
    String pathFor(const char* file) const;

    fs::FS* fs_{nullptr};
    String root_{"/"};
    bool loaded_{false};
    std::vector<LightPattern> patterns_;
    String activePatternId_;
};
