#include "WebInterfaceManager.h"
#include "Globals.h"
#include "SDManager.h"
#include "WiFiManager.h"
#include "OTAManager.h"
#include "SDVoting.h"
#include <ESPAsyncWebServer.h>

static AsyncWebServer server(80);

void handleRoot(AsyncWebServerRequest *request)
{
  if (!isSDReady())
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
  setWebBrightness(val / 255.0f);
  request->send(200, "text/plain", "OK");
  PF("[Web] Brightness ingesteld op %.2f\n", getWebBrightness());
}

void handleGetBrightness(AsyncWebServerRequest *request)
{
  int level = (int)(getWebBrightness() * 255.0f);
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
  setWebAudioLevel(val);
  request->send(200, "text/plain", "OK");
  PF("[Web] AudioLevel ingesteld op %.2f\n", val);
}

void handleGetWebAudioLevel(AsyncWebServerRequest *request)
{
  float val = getWebAudioLevel();
  request->send(200, "text/plain", String(val, 2));
}

/* ----- OTA handlers ----- */

void handleOtaArm(AsyncWebServerRequest *request)
{
  otaArm(300);
  request->send(200, "text/plain", "OK");
}

void handleOtaConfirm(AsyncWebServerRequest *request)
{
  if (otaConfirmAndReboot())
  {
    request->send(200, "text/plain", "REBOOTING");
  }
  else
  {
    request->send(400, "text/plain", "EXPIRED");
  }
}

/* ----- setup/loop glue ----- */

void beginWebInterface()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setBrightness", HTTP_GET, handleSetBrightness);
  server.on("/getBrightness", HTTP_GET, handleGetBrightness);
  server.on("/setWebAudioLevel", HTTP_GET, handleSetWebAudioLevel);
  server.on("/getWebAudioLevel", HTTP_GET, handleGetWebAudioLevel);

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
