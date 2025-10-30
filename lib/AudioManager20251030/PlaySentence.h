//PlaySentence.h
#pragma once
#include <Arduino.h>

namespace PlaySentence
{

// Maximale aantal woorden per zin
#define MAX_WORDS_PER_SENTENCE 20
#define END_OF_SENTENCE 255
#define WORD_INTERVAL_MS 10
#define WORD_EXTRA_FACTOR  0.12f   //  extra speeltijd
#define WORD_EXTRA_MIN  70      // extra altijd minstens .. ms
#define WORD_MIN_MS    10   // minimale speelduur

  void say(const uint8_t word);
  void start(const uint8_t* words);
  void startTTS(const String& text);

    void update();
    void stop();

}
