#include <Arduino.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>

#include "Globals.h"         // gives HWconfig (SSID/PASS/OTA_URL if you have it there)
#include "WifiManager.h"     // bootWiFiConnect(), waitForNetwork()
#include "OTAManager.h"

static Preferences prefs; // NVS ns="ota"

static uint8_t  nvs_get8 (const char* k){ prefs.begin("ota", true);  uint8_t  v=prefs.getUChar(k,0); prefs.end(); return v; }
static void     nvs_set8 (const char* k,uint8_t v){ prefs.begin("ota", false); prefs.putUChar(k,v); prefs.end(); }
static uint32_t nvs_get32(const char* k){ prefs.begin("ota", true); uint32_t v=prefs.getUInt(k,0); prefs.end(); return v; }
static void     nvs_set32(const char* k,uint32_t v){ prefs.begin("ota", false); prefs.putUInt(k,v); prefs.end(); }

static bool httpStreamToUpdate(HTTPClient& http){
  WiFiClient *stream = http.getStreamPtr();
  int len = http.getSize();
  if (len <= 0) return false;
  if (!Update.begin(len)) return false;

  uint32_t start = millis(), lastData = millis();
  uint8_t buf[2048];

  while (http.connected() && (len > 0 || len == -1)) {
    size_t avail = stream->available();
    if (avail) {
      size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
      if (!n) break;
      if (Update.write(buf, n) != n) return false;
      lastData = millis();
      if (len > 0) len -= n;
    } else {
      if (millis() - lastData > 30000) return false;           // 30 s stall
      delay(10);
    }
    if (millis() - start > 5UL*60UL*1000UL) return false;      // 5 min cap
  }
  if (!Update.end()) return false;
  return Update.isFinished();
}

static void runOtaSetup(){
  PL("[OTA] start");
  bootWiFiConnect();
  if (WiFi.status() != WL_CONNECTED){ PL("[OTA] no WiFi"); return; }

  HTTPClient http;
  http.setTimeout(30000);
if (!http.begin(String(OTA_URL))) { PF("[OTA] bad URL: %s\n", OTA_URL); return; }
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    if (httpStreamToUpdate(http)) {
      PL("[OTA] OK, reboot");
      http.end();
      delay(100);
      ESP.restart();
    } else {
      PL("[OTA] failed");
    }
  } else {
    PF("[OTA] HTTP %d\n", code);
  }
  http.end();
}

void otaArm(uint32_t window_s){
  const uint32_t nowS = millis()/1000;
  nvs_set32("armed_until", nowS + window_s);
  nvs_set8("mode", 1);   // 0=normal,1=pending,2=ota
  PL("[OTA] armed");
}

bool otaConfirmAndReboot(){
  const uint8_t mode = nvs_get8("mode");
  const uint32_t nowS = millis()/1000;
  const uint32_t arm  = nvs_get32("armed_until");
  if (mode == 1 && nowS <= arm){
    nvs_set8("mode", 2);
    PL("[OTA] confirm -> reboot");
    delay(50);
    ESP.restart();
    return true; // not reached
  }
  nvs_set8("mode", 0);
  PL("[OTA] confirm expired");
  return false;
}

void otaBootDispatcher(){
  const uint8_t mode = nvs_get8("mode");

  if (mode == 2){
    nvs_set8("mode", 0);        // clear first to avoid loops
    runOtaSetup();
    ESP.restart();
  }

  if (mode == 1){
    const uint32_t nowS = millis()/1000;
    const uint32_t arm  = nvs_get32("armed_until");
    if (nowS > arm){
      nvs_set8("mode", 0);
      PL("[OTA] window expired");
    }
  }
}
/*
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>MyKwal Webinterface</title>
  <style>
    body { font-family: sans-serif; background:#f2f2f2; color:#333; padding:2em; max-width:500px; margin:auto; }
    h1 { text-align:center; font-size:1.5em; }
    .row { margin-bottom:1.5em; }
    label { display:block; margin-bottom:0.4em; font-weight:bold; }
    input[type="range"] { width:100%; }
    .value { font-size:0.9em; color:#666; text-align:right; }
    button { padding:0.6em 1em; margin-right:0.5em; }
    #status { margin-top:1em; font-size:0.9em; color:#444; white-space:pre-line; }
  </style>
</head>
<body>
  <h1>MyKwal Interface</h1>

  <div class="row">
    <label for="brightnessSlider">Brightness</label>
    <input type="range" id="brightnessSlider" min="0" max="255"/>
    <div class="value" id="brightnessValue">...</div>
  </div>

  <div class="row">
    <label for="audioSlider">Audio Volume</label>
    <input type="range" id="audioSlider" min="0" max="100"/>
    <div class="value" id="audioValue">...</div>
  </div>

  <hr/>

  <div class="row">
    <label>OTA</label>
    <button id="armBtn" type="button">Get ready for OTA (5 min)</button>
    <button id="confirmBtn" type="button">Confirm OTA</button>
    <div id="status"></div>
  </div>

  <script>
    const brightnessSlider = document.getElementById("brightnessSlider");
    const brightnessValue  = document.getElementById("brightnessValue");
    const audioSlider      = document.getElementById("audioSlider");
    const audioValue       = document.getElementById("audioValue");
    const armBtn           = document.getElementById("armBtn");
    const confirmBtn       = document.getElementById("confirmBtn");
    const statusDiv        = document.getElementById("status");

    function setStatus(txt){ statusDiv.textContent = txt; }

    // Brightness
    brightnessSlider.addEventListener("input", () => {
      fetch(`/setBrightness?value=${brightnessSlider.value}`);
      brightnessValue.textContent = brightnessSlider.value;
    });

    // Audio
    audioSlider.addEventListener("input", () => {
      const v = audioSlider.value / 100.0;
      fetch(`/setWebAudioLevel?value=${v.toFixed(2)}`);
      audioValue.textContent = v.toFixed(2);
    });

    // Init from device
    fetch("/getBrightness")
      .then(r => r.text())
      .then(v => { brightnessSlider.value = v; brightnessValue.textContent = v; })
      .catch(()=>{});

    fetch("/getWebAudioLevel")
      .then(r => r.text())
      .then(t => { const v = parseFloat(t)||0; audioSlider.value = Math.round(v*100); audioValue.textContent = v.toFixed(2); })
      .catch(()=>{});

    // OTA actions
    armBtn.addEventListener("click", () => {
      setStatus("Arming OTA window…");
      fetch("/ota/arm")
        .then(r => r.text())
        .then(t => setStatus(`OTA arm: ${t}`))
        .catch(e => setStatus(`OTA arm error`));
    });

    confirmBtn.addEventListener("click", () => {
      setStatus("Confirming OTA… device will reboot if accepted.");
      fetch("/ota/confirm", { method: "POST" })
        .then(async r => {
          const t = await r.text();
          if (r.ok) setStatus(t); else setStatus(`Error: ${t}`);
        })
        .catch(() => setStatus("OTA confirm error"));
    });
  </script>
</body>
</html>

*/