#pragma once

#include <Arduino.h>

float getAudioLevel();
void setAudioLevel(float value);
float getWebAudioLevel();
void setWebAudioLevel(float value);
float getTimeAudioLevel();
void setTimeAudioLevel(float value);
void setAudioLevelRaw(int16_t value);
int16_t getAudioLevelRaw();
float getBaseGain();
void setBaseGain(float value);
float getEffectiveAudioLevel();
bool isAudioBusy();
void setAudioBusy(bool value);
bool getCurrentDirFile(uint8_t& dir, uint8_t& file);
void setCurrentDirFile(uint8_t dir, uint8_t file);
bool isFragmentPlaying();
void setFragmentPlaying(bool value);
bool isSentencePlaying();
void setSentencePlaying(bool value);
bool isTtsActive();
void setTtsActive(bool value);
bool isWordPlaying();
void setWordPlaying(bool value);
int32_t getCurrentWordId();
void setCurrentWordId(int32_t value);
