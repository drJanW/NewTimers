// PlaySentence.cpp

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <AudioFileSourceHTTPStream.h>
#include <AudioGeneratorMP3.h>

#include "AudioManager.h"
#include "PlaySentence.h"
#include "Globals.h"

// --------- forward declarations om link-/scope-fouten te voorkomen ----------
namespace PlayAudioFragment { void stop(); } // bestaand in jouw project

// Deze drie bleken undeclared in jouw buildlog:
extern bool ttsActive;
extern bool wordPlaying;
extern int32_t currentWordID;

// ---------------- VoiceRSS probe: detecteer "ERROR:" vóór streamen -----------
namespace PlaySentence {

static AudioManager& audio() { return AudioManager::instance(); }

    void stop() {
    if (audio().audioMp3Decoder) {
        audio().audioMp3Decoder->stop();
        delete audio().audioMp3Decoder;
        audio().audioMp3Decoder = nullptr;
    }
    if (audio().audioFile) {
        delete audio().audioFile;
        audio().audioFile = nullptr;
    }
    setSentencePlaying(false);
    setAudioBusy(false);
    ttsActive = false;
}

void update() {
    if (!audio().audioMp3Decoder) return;
    if (audio().audioMp3Decoder->isRunning()) {
        if (!audio().audioMp3Decoder->loop()) {
            audio().audioMp3Decoder->stop();
        }
    } else {
        stop();
    }
}


static bool voicerss_ok(const String& url, String& err) {
    HTTPClient http;
    WiFiClient client;

    http.setTimeout(5000);
    if (!http.begin(client, url)) { err = "init failed"; return false; }

    // Zorg dat Content-Type beschikbaar is na GET
    const char* keys[] = {"Content-Type"};
    http.collectHeaders(keys, 1);

    http.addHeader("Range", "bytes=0-255");   // desgewenst weghalen als onnodig
    int code = http.GET();
    String ct = http.header("Content-Type");

    String head;
    WiFiClient* s = http.getStreamPtr();
    uint32_t t0 = millis();
    while (s->connected() && s->available() == 0 && (millis() - t0) < 2000) delay(5);
    while (s->available() && head.length() < 256) head += char(s->read());
    http.end();

    if ((code == 200 || code == 206) && ct.startsWith("audio/")) return true;
    if (head.startsWith("ERROR")) { err = head; return false; }
    err = String("HTTP ") + code + " CT:" + ct;
    return false;
}


String makeVoiceRSSUrl(const String &text)
{
    return String("http://api.voicerss.org/?key=") + VOICERSS_API_KEY +
           "&hl=nl-nl&v=Bram&c=MP3&f=44khz_16bit_mono&src=" + urlencode(text);
}

void startTTS(const String &text)
{
    // Prioriteit: like start(): breek fragment af, skip als zin al speelt
    if (isAudioBusy())
    {
        if (isFragmentPlaying())
        {
            PlayAudioFragment::stop();
            setFragmentPlaying(false);
            setAudioBusy(false);
        }
        else if (isSentencePlaying())
        {
            return;
        }
    }

    // Vereist WiFi
    if (!isWiFiConnected())
    {
        PF("[TTS] No WiFi – cannot start TTS\n");
        return;
    }

    // Cleanup eventuele restanten
    stop();

    setAudioBusy(true);
    setSentencePlaying(true);
    ttsActive = true;
    wordPlaying = false;
    currentWordID = END_OF_SENTENCE;

    float g = getBaseGain() * 1.8f; // tweak naar smaak (1.2–1.8)
    if (g > 1.0f) g = 1.0f;
    audio().audioOutput.SetGain(g);

    // Eerst checken op ERROR-respons van VoiceRSS
    String url = makeVoiceRSSUrl(text);
    {
        String apiErr;
        if (!voicerss_ok(url, apiErr)) {
            PF("[TTS] VoiceRSS error: %s\n", apiErr.c_str());
            setAudioBusy(false);
            setSentencePlaying(false);
            ttsActive = false;
            return;
        }
    }

    // Dan pas streamen en decoderen
    audio().audioFile = new AudioFileSourceHTTPStream(url.c_str());
    audio().audioMp3Decoder = new AudioGeneratorMP3();
    audio().audioMp3Decoder->begin(audio().audioFile, &audio().audioOutput);

    // Geen timer nodig; AudioManager::update() pompt en ruimt op bij einde.
    PF("[TTS] Started: %s\n", text.c_str());
}

} // namespace PlaySentence
