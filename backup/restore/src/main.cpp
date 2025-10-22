#include <Arduino.h>
#include <WiFi.h>
#include <FastLED.h>
#include <SD.h>
#include "Globals.h"
#include "TimerManager.h"
#include "WiFiManager.h"

// === CONFIG ===
//#define LED_PIN         PIN_RGB
//#define NUM_LEDS        200
//#define BUTTON_PIN      12
#define HEARTBEAT_PIN    LED_BUILTIN
#define AUDIO_BUFFER_SIZE 512

// === GLOBALS ===
CRGB leds[NUM_LEDS];
uint8_t hue = 0;
bool buttonPressed = false;

extern TimerManager timers;

// === CALLBACKS ===
void cb_heartbeat() {
  digitalWrite(HEARTBEAT_PIN, !digitalRead(HEARTBEAT_PIN));
}

void cb_refreshDisplay() {
  FastLED.show();
}

void cb_animateLEDs() {
  fill_rainbow(leds, NUM_LEDS, hue++, 7);
}

void cb_sampleSensor() {
  int value = analogRead(A0);
  if (value > 0) {
    Serial.print("Sensor: ");
    Serial.println(value);
  }
}

void cb_buttonDebounced() {
  buttonPressed = true;
  Serial.println("Button confirmed");
}

void cb_audioTick() {
  static uint8_t buffer[AUDIO_BUFFER_SIZE];
  File audioFile = SD.open("/audio.raw");
  if (audioFile) {
    audioFile.read(buffer, AUDIO_BUFFER_SIZE);
    audioFile.close();
    Serial.println("Audio tick");
  }
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  pinMode(HEARTBEAT_PIN, OUTPUT);

    PL("\n=== BOOT ===\n");
//  FastLED.addLeds<WS2812, PIN_RGB, GRB>(leds, NUM_LEDS);
  WiFi.mode(WIFI_STA);
  SD.begin();

 if (!timers.create(1000, 0, cb_heartbeat)) {
  Serial.println("Failed to create heartbeat timer");
}
 // timers.create(50,   0, cb_refreshDisplay);    // 20 Hz display refresh
 // timers.create(40,   0, cb_animateLEDs);       // 25 Hz animation logic
 // timers.create(200,  0, cb_sampleSensor);      // 5 Hz sensor sampling
 // timers.create(10,   0, cb_audioTick);         // 100 Hz audio tick

 startWiFiConnect();  // non-blocking Wi-Fi logic via TimerManager
}

// === LOOP ===
void loop() {
  timers.update();

}