#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "ContextModels.h"

bool HexToRgb(const String& hex, RgbColor& out);

class LightColorStore {
public:
    bool begin(fs::FS& sd, const char* rootPath = "/");
    bool ready() const;
    const LightColor* find(uint8_t id) const;
    const LightColor* active() const;
    void clear();

private:
    bool load();
    String pathFor(const char* file) const;

    fs::FS* fs_{nullptr};
    String root_{"/"};
    bool loaded_{false};
    std::vector<LightColor> colors_;
    uint8_t activeColorId_{0};
};
