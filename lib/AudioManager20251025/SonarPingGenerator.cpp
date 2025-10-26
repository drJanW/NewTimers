#include "SonarPingGenerator.h"

#include "Globals.h"
#include <math.h>
#include <stdlib.h>

namespace {
constexpr float TAU_F = 6.28318530717958647692f;
constexpr float ATTACK_MS = 35.0f;
constexpr float DECAY_MS = 260.0f;
constexpr float HARMONIC_GAIN = 0.18f;
constexpr uint16_t BATCH_SAMPLES = 32;

float clampUnit(float v) {
    if (v < -1.0f) return -1.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float fastRand() {
    return static_cast<float>((random() & 0xFFFF) - 0x8000) / 32768.0f;
}

} // namespace

SonarPingGenerator::SonarPingGenerator() {
    running = false;
    output = nullptr;
    file = nullptr;
    index_ = 0;
    totalSamples_ = 0;
    phase_ = 0.0f;
    stallLogged_ = false;
    stallCounter_ = 0;
}

void SonarPingGenerator::configure(const SonarPingProfile &profile) {
    cfg_ = profile;
    if (cfg_.durationMs < 10.0f) {
        cfg_.durationMs = 10.0f;
    }
    if (cfg_.amplitude < 0.01f) {
        cfg_.amplitude = 0.01f;
    }
    if (cfg_.amplitude > 1.0f) {
        cfg_.amplitude = 1.0f;
    }
    if (cfg_.noise < 0.0f) {
        cfg_.noise = 0.0f;
    }
    if (cfg_.noise > 1.0f) {
        cfg_.noise = 1.0f;
    }
}

bool SonarPingGenerator::begin(AudioFileSource *source, AudioOutput *out) {
    (void)source;
    output = out;
    if (!output) {
        return false;
    }

    sampleRate_ = 44100.0f;
    output->SetRate(static_cast<int>(sampleRate_));
    output->SetBitsPerSample(16);
    output->SetChannels(2);
    if (!output->begin()) {
        return false;
    }

    index_ = 0;
    phase_ = 0.0f;
    const float samples = cfg_.durationMs * sampleRate_ / 1000.0f;
    totalSamples_ = samples > 1.0f ? static_cast<uint32_t>(samples) : 1U;
    running = true;
    stallLogged_ = false;
    stallCounter_ = 0;
    PF("[SonarGen] begin dur=%.1fms samples=%lu start=%.1fHz end=%.1fHz amp=%.2f noise=%.2f\n",
       static_cast<double>(cfg_.durationMs),
       static_cast<unsigned long>(totalSamples_),
       static_cast<double>(cfg_.startHz),
       static_cast<double>(cfg_.endHz),
       static_cast<double>(cfg_.amplitude),
       static_cast<double>(cfg_.noise));
    return true;
}

bool SonarPingGenerator::loop() {
    if (!running || !output) {
        return false;
    }

    uint16_t produced = 0;
    int16_t frame[2];

    while (produced < BATCH_SAMPLES && index_ < totalSamples_) {
        if (!renderSample(frame)) {
            running = false;
            break;
        }

        bool pushed = output->ConsumeSample(frame);

        if (!pushed) {
            constexpr uint8_t kMaxAttempts = 6;
            for (uint8_t attempt = 0; attempt < kMaxAttempts && !pushed; ++attempt) {
                if (output) {
                    output->loop();
                }
                yield();
                pushed = output->ConsumeSample(frame);
            }
        }

        if (!pushed) {
            ++stallCounter_;
            if (!stallLogged_ || stallCounter_ % 16U == 0U) {
                PF("[SonarGen] buffer stalled at idx=%lu (cnt=%u)\n",
                   static_cast<unsigned long>(index_),
                   static_cast<unsigned>(stallCounter_));
                stallLogged_ = true;
            }
            break;
        }

        stallLogged_ = false;
        stallCounter_ = 0;

        ++index_;
        ++produced;
    }

    if (index_ >= totalSamples_) {
        stop();
        return false;
    }

    if (output) {
        output->loop();
    }

    return running;
}

bool SonarPingGenerator::stop() {
    if (!running) {
        return false;
    }
    running = false;
    PF("[SonarGen] stop at idx=%lu (total=%lu)\n",
       static_cast<unsigned long>(index_),
       static_cast<unsigned long>(totalSamples_));
    stallCounter_ = 0;
    if (output) {
        output->flush();
        output->stop();
    }
    return false;
}

uint32_t SonarPingGenerator::remainingMs() const {
    if (!running) {
        return 0;
    }
    if (sampleRate_ <= 0.0f) {
        return 0;
    }
    if (index_ >= totalSamples_) {
        return 0;
    }
    const float remainingSamples = static_cast<float>(totalSamples_ - index_);
    const float remainingMs = (remainingSamples / sampleRate_) * 1000.0f;
    if (remainingMs <= 0.0f) {
        return 0;
    }
    return static_cast<uint32_t>(remainingMs + 0.5f);
}

bool SonarPingGenerator::renderSample(int16_t (&frame)[2]) {
    const float t = static_cast<float>(index_) / sampleRate_;
    const float durationSec = cfg_.durationMs / 1000.0f;
    const float progress = durationSec > 0.0f ? t / durationSec : 1.0f;
    const float sweep = cfg_.startHz + (cfg_.endHz - cfg_.startHz) * progress;
    const float freq = sweep > 0.0f ? sweep : cfg_.startHz;

    phase_ += TAU_F * freq / sampleRate_;
    if (phase_ > TAU_F) {
        phase_ = fmodf(phase_, TAU_F);
    }

    const float attackSec = ATTACK_MS / 1000.0f;
    const float decaySec = DECAY_MS / 1000.0f;
    const float attack = 1.0f - expf(-t / attackSec);
    const float decay = expf(-t / decaySec);
    float envelope = attack * decay;
    if (envelope < 0.0f) envelope = 0.0f;
    if (envelope > 1.0f) envelope = 1.0f;

    float signal = sinf(phase_);
    signal += HARMONIC_GAIN * sinf(phase_ * 2.0f);
    signal += cfg_.noise * fastRand();

    float sample = cfg_.amplitude * envelope * signal;
    sample = clampUnit(sample);
    const int16_t value = static_cast<int16_t>(sample * 32767.0f);
    frame[0] = value;
    frame[1] = value;
    return true;
}
