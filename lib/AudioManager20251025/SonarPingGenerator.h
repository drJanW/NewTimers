#pragma once

#include <Arduino.h>
#include "AudioGenerator.h"

struct SonarPingProfile {
    float startHz = 800.0f;
    float endHz = 1100.0f;
    float durationMs = 320.0f;
    float amplitude = 0.4f;
    float noise = 0.0f;
};

class SonarPingGenerator : public AudioGenerator {
public:
    SonarPingGenerator();

    void configure(const SonarPingProfile &profile);

    bool begin(AudioFileSource *source, AudioOutput *output) override;
    bool loop() override;
    bool stop() override;
    bool isRunning() override { return running; }
    uint32_t remainingMs() const;

private:
    bool renderSample(int16_t (&frame)[2]);

    SonarPingProfile cfg_{};
    float sampleRate_ = 44100.0f;
    uint32_t totalSamples_ = 0;
    uint32_t index_ = 0;
    float phase_ = 0.0f;
    bool stallLogged_ = false;
    uint16_t stallCounter_ = 0;
};
