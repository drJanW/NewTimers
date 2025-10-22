#include "NumberSpeaker.h"
#include "PlaySentence.h"
#include "Globals.h"

static inline void speak(const String& s) {
  if (!isWiFiConnected()) { PF("[TTS] No WiFi: %s\n", s.c_str()); return; }
  PlaySentence::startTTS(s);
}

void SayNumber(uint32_t number) {
  //if (number > 1500000000UL) {    speak("Fout. Over één en een half miljard. Fout.");    return;  }
  speak(String(number));
}

void SayNumberText(const String& prefix, uint32_t number) {
  if (number > 1500000000UL) {
    speak(prefix + " fout. Over één en een half miljard. Fout.");
    return;
  }
  speak(prefix + " " + String(number));
}
