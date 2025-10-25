// HWconfig.h
// Updated on 20250922
// this file should not be changed by chatGPT/Copilot/DeepSeek/Claude/LeChat

#ifndef HWCONFIG_H
#define HWCONFIG_H

#include <Arduino.h>
//#include "config_secrets.h"
// #include "topoLEDMap.h" --> now as .bin on SDcard

// ======================= Pin Definitions =======================
#define LED_PIN 2 // Built-in status LED
#define LED_BUILTIN LED_PIN
#define PIN_LED LED_PIN
#define PIN_RGB 4       // LED Data Output
#define PIN_I2S_DOUT 14 // I2S Data
#define PIN_I2S_BCLK 13 // I2S Bit Clock
#define PIN_I2S_LRC 15  // I2S Word Select (LR Clock)
#define PIN_SD_CS 5     // SD Card Chip Select

// SPI Pins (VSPI)
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

#define SPI_HZ 16000000

// I2C Pins (shared by sensors like BH1750)
#define SDA 21
#define SCL 22
#define I2C_SDA 21
#define I2C_SCL 22

#define OTA_TRIG_PIN 33

// ======================= LED Configuration =======================
#define NUM_LEDS 160      // Current LED count in final dome
#define LED_TYPE WS2812B  // LED type used
#define LED_RGB_ORDER GRB // Color order
#define MAX_BRIGHTNESS 200
#define MAX_VOLTS 6
#define MAX_MILLIAMPS 1200
// ======================= Audio Configuration =======================
#define MAX_AUDIO_VOLUME 0.2f // Maximum audio output volume

// ======================= Static IP Configuration =======================
#define USE_STATIC_IP true // zet op false voor DHCP

#define WIFI_SSID "keijebijter"
#define WIFI_PASSWORD "Helmondia'55"
#define OTA_PASSWORD "KwalOTA_3732"
#define OTA_URL "http://192.168.178.2/firmware.bin"
#define VOICERSS_API_KEY "9889993b45294559968a1c26c59bc1d1"

#define STATIC_IP_ADDRESS 192, 168, 178, 188
#define STATIC_GATEWAY 192, 168, 178, 1
#define STATIC_SUBNET 255, 255, 255, 0
#define STATIC_DNS 8, 8, 8, 8

#endif // HWCONFIG_H
