#include "AudioPolicy.h"
#include "Globals.h" // only for constants like MAX_AUDIO_VOLUME

namespace AudioPolicy {

bool canPlayFragment() {
    // Rule: fragments only if audio not busy and no TTS active
    return !AudioManager::instance().isBusy()
        && !AudioManager::instance().isSentencePlaying();
}

bool canPlaySentence() {
    // Rule: TTS always wins; can interrupt fragment
    return true;
}

float applyVolumeRules(float requested, bool quietHours) {
    float v = requested;
    if (v < 0.0f) v = 0.0f;
    if (v > MAX_AUDIO_VOLUME) v = MAX_AUDIO_VOLUME;
    if (quietHours && v > 0.3f) v = 0.3f; // cap volume at night
    return v;
}

void requestFragment(const AudioFragment& frag) {
    if (canPlayFragment()) {
        AudioManager::instance().startFragment(frag);
    }
    // else: could queue or ignore
}

void requestSentence(const String& phrase) {
    if (canPlaySentence()) {
        AudioManager::instance().startTTS(phrase);
    }
}

}
