#include "WebInterfaceManager.h"
#include "Globals.h"
#include "LightManager.h"
#include "SDManager.h"
#include "WiFiManager.h"
#include "ConductManager.h"
#include "AudioManager.h"
#include "SDVoting.h"
#include <ESPAsyncWebServer.h>

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

static AsyncWebServer server(80);

void handleRoot(AsyncWebServerRequest *request)
{
  if (!SDManager::isReady())
  {
    request->send(500, "text/plain", "SD init failed");
    return;
  }
  if (!SD.exists("/index.html"))
  {
    request->send(500, "text/plain", "index.html niet gevonden");
    return;
  }
  request->send(SD, "/index.html", "text/html");
}

void handleSetBrightness(AsyncWebServerRequest *request)
{
  if (!request->hasParam("value"))
  {
    request->send(400, "text/plain", "Missing ?value");
    return;
  }
  String valStr = request->getParam("value")->value();
  int val = valStr.toInt();
  val = constrain(val, 0, 255);
  const float webFactor = static_cast<float>(val) / 255.0f;
  setWebBrightness(webFactor);
  ConductManager::intentSetBrightness(static_cast<float>(val));
  request->send(200, "text/plain", "OK");
  WEBIF_LOG("[Web] Brightness ingesteld op %d (web %.2f)\n", val, webFactor);
}

void handleGetBrightness(AsyncWebServerRequest *request)
{
  int level = static_cast<int>(getWebBrightness() * 255.0f);
  request->send(200, "text/plain", String(level));
}

void handleSetWebAudioLevel(AsyncWebServerRequest *request)
{
  if (!request->hasParam("value"))
  {
    request->send(400, "text/plain", "Missing ?value");
    return;
  }
  String valStr = request->getParam("value")->value();
  float val = valStr.toFloat();
  val = constrain(val, 0.0f, 1.0f);
  ConductManager::intentSetAudioLevel(val);
  request->send(200, "text/plain", "OK");
  WEBIF_LOG("[Web] AudioLevel ingesteld op %.2f\n", AudioManager::instance().getWebLevel());
}

void handleGetWebAudioLevel(AsyncWebServerRequest *request)
{
  float val = AudioManager::instance().getWebLevel();
  request->send(200, "text/plain", String(val, 2));
}

/* ----- OTA handlers ----- */

void handleOtaArm(AsyncWebServerRequest *request)
{
  ConductManager::intentArmOTA(300);
  request->send(200, "text/plain", "OK");
}

void handleOtaConfirm(AsyncWebServerRequest *request)
{
  if (ConductManager::intentConfirmOTA())
  {
    request->send(200, "text/plain", "REBOOTING");
  }
  else
  {
    request->send(400, "text/plain", "EXPIRED");
  }
}

void handleLightNext(AsyncWebServerRequest *request)
{
  ConductManager::intentNextLightShow();
  request->send(200, "text/plain", "LIGHT NEXT");
}

/* ----- setup/loop glue ----- */

void beginWebInterface()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setBrightness", HTTP_GET, handleSetBrightness);
  server.on("/getBrightness", HTTP_GET, handleGetBrightness);
  server.on("/setWebAudioLevel", HTTP_GET, handleSetWebAudioLevel);
  server.on("/getWebAudioLevel", HTTP_GET, handleGetWebAudioLevel);
  server.on("/light/next", HTTP_GET, handleLightNext);

  // OTA routes
  server.on("/ota/arm", HTTP_GET, handleOtaArm);
  server.on("/ota/confirm", HTTP_POST, handleOtaConfirm);

  SDVoting::attachVoteRoute(server);

  server.begin();
  PF("[WebInterface] IP address: %s\n", WiFi.localIP().toString().c_str());
}

void updateWebInterface()
{
  // AsyncWebServer: niet nodig. Leeg laten voor compatibiliteit.
}
