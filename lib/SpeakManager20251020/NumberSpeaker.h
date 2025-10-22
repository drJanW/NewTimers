#pragma once
#include <Arduino.h>

void SayNumber(uint32_t number);
void SayNumberText(const String& prefix, uint32_t number); // optioneel: met tekst
