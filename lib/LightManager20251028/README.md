LightManager Struct-API Architectuur (Deepseek-style)

Elke LightShow gebruikt:

Universele struct: LightShowParams (kleur, cycli, type)

Show-specifieke struct: ExtraXXParams (alleen in LightManager.h)

Dispatchers: PlayXXShow() in LightManager.cpp

Geen eigen timers, geen millis(), geen struct-definities in show-headers

Nieuwe LightShow toevoegen: Werkwijze

Kopieer EmptyShow.cpp en EmptyShow.h als basis. Hernoem naar jouw show (bv FireflyShow).

Maak een struct ExtraFireflyParams in LightManager.h

Voeg aan enum LightShow in LightManager.h je nieuwe show toe.

Voeg dispatcher toe in LightManager.h: void PlayFireflyShow(const LightShowParams&, const ExtraFireflyParams& = dummyFireflyParams);

Voeg include toe in LightManager.h: #include "FireflyShow.h"

Implementeer dispatcher in LightManager.cpp (zie voorbeeld)

Voeg aan switch/case van updateLightManager() en PlayLightShow() jouw show toe.

Skeleton: EmptyShow.h

#ifndef EMPTYSHOW_H
#define EMPTYSHOW_H

#include "LightManager.h"
#include <FastLED.h>

extern CRGB leds[];

void initEmptyShow(const LightShowParams& p, const ExtraEmptyParams& extra = dummyEmptyParams);
void updateEmptyShow();

#endif

Skeleton: EmptyShow.cpp

#include "EmptyShow.h"

static ExtraEmptyParams emptyParams = {};
const ExtraEmptyParams dummyEmptyParams = ExtraEmptyParams();

void initEmptyShow(const LightShowParams& p, const ExtraEmptyParams& extra) {
emptyParams = extra;
// Initialisatiecode hier
}

void updateEmptyShow() {
// Gebruik cycli via getColorPhase() / getBrightPhase()
for (int i = 0; i < NUM_LEDS; i++) {
leds[i] = CRGB::Black;
}
FastLED.show();
}

Declaratie/aanroepen

In LightManager.h:

struct ExtraEmptyParams { ... } // ALLEEN hier!

enum LightShow { ..., emptyShow }

void PlayEmptyShow(const LightShowParams&, const ExtraEmptyParams& = dummyEmptyParams);

#include "EmptyShow.h"

In LightManager.cpp:

void PlayEmptyShow(const LightShowParams&, const ExtraEmptyParams&)

case emptyShow: updateEmptyShow(); break; // in updateLightManager()

case emptyShow: PlayEmptyShow(p); break; // in PlayLightShow()

Samengevat:

Structs/enums nooit in show-headers

Alleen functieprototypes in show-headers

Geen eigen timers in shows, alles via centrale cycli

Test alles na toevoegen

Dit skelet voorkomt spaghetti en houdt alles schaalbaar en onderhoudbaar.