#include "WebInterfaceManager.h"
#include "Globals.h"
#include "LightManager.h"
#include "SDManager.h"
#include "WiFiManager.h"
#include "ConductManager.h"
#include "AudioManager.h"
#include "SDVoting.h"
#include "WebLightStore.h"
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <WiFi.h>
#include <memory>

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 1
#endif

#if WEBIF_LOG_LEVEL
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#else
#define WEBIF_LOG(...) do {} while (0)
#endif

static AsyncWebServer server(80);

namespace {

constexpr size_t kMaxSdEntries = 256;

void ensureLightStoreReady()
{
  if (!WebLightStore::instance().isReady())
  {
    WebLightStore::instance().begin();
  }
}

void sendJsonResponse(AsyncWebServerRequest *request, String payload, const char *extraHeader = nullptr, const String &extraValue = String())
{
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
  response->addHeader("Cache-Control", "no-store");
  if (extraHeader && extraHeader[0] != '\0')
  {
    response->addHeader(extraHeader, extraValue);
  }
  request->send(response);
}

void sendError(AsyncWebServerRequest *request, int code, const String &message)
{
  AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", message);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void appendJsonEscaped(String &out, const char *value)
{
  if (!value)
  {
    return;
  }
  while (*value)
  {
    const char c = *value++;
    switch (c)
    {
    case '\\':
      out += F("\\\\");
      break;
    case '"':
      out += F("\\\"");
      break;
    case '\n':
      out += F("\\n");
      break;
    case '\r':
      out += F("\\r");
      break;
    case '\t':
      out += F("\\t");
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        // Control char: skip to avoid corrupting JSON payload.
        break;
      }
      out += c;
      break;
    }
  }
}

String sanitizeSdPath(const String &raw)
{
  if (raw.length() == 0)
  {
    return String("/");
  }

  String path = raw;
  path.trim();
  if (path.length() == 0)
  {
    return String("/");
  }

  if (path[0] != '/')
  {
    path = "/" + path;
  }

  while (path.length() > 1 && path.endsWith("/"))
  {
    path.remove(path.length() - 1);
  }

  if (path.indexOf("..") >= 0)
  {
    return String();
  }

  if (path.length() >= SDPATHLENGTH)
  {
    return String();
  }

  return path;
}

String buildParentPath(const String &path)
{
  if (path.length() <= 1)
  {
    return String("/");
  }
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash <= 0)
  {
    return String("/");
  }
  String parent = path.substring(0, lastSlash);
  if (parent.length() == 0)
  {
    return String("/");
  }
  return parent;
}

struct SdListAccumulator
{
  String entries;
  size_t count = 0;
  bool first = true;
};

void sdListCollectCallback(const char *name, bool isDirectory, uint32_t sizeBytes, void *context)
{
  if (!context)
  {
    return;
  }
  auto *acc = static_cast<SdListAccumulator *>(context);
  if (!acc->first)
  {
    acc->entries += ',';
  }
  acc->first = false;
  acc->entries += F("{\"name\":\"");
  appendJsonEscaped(acc->entries, name);
  acc->entries += F("\",\"type\":\"");
  acc->entries += isDirectory ? F("dir") : F("file");
  acc->entries += F("\",\"size\":");
  acc->entries += String(sizeBytes);
  acc->entries += F("}");
  acc->count++;
}

void sendSdStatus(AsyncWebServerRequest *request)
{
  const bool ready = SDManager::isReady();
  const bool busy = SDManager::isBusy();
  const bool hasIndex = ready && SDManager::instance().fileExists("/index.html");

  String payload = F("{");
  payload += F("\"ready\":");
  payload += ready ? F("true") : F("false");
  payload += F(",\"busy\":");
  payload += busy ? F("true") : F("false");
  payload += F(",\"hasIndex\":");
  payload += hasIndex ? F("true") : F("false");
  payload += F("}");

  sendJsonResponse(request, payload);
}

void sendSdList(AsyncWebServerRequest *request)
{
  if (!SDManager::isReady())
  {
    sendError(request, 503, F("SD not ready"));
    return;
  }

  const String rawPath = request->hasParam("path") ? request->getParam("path")->value() : String("/");
  const String path = sanitizeSdPath(rawPath);
  if (path.length() == 0)
  {
    sendError(request, 400, F("Invalid path"));
    return;
  }

  SdListAccumulator accumulator;
  bool truncated = false;
  const bool ok = SDManager::instance().listDirectory(path.c_str(), sdListCollectCallback, &accumulator, kMaxSdEntries, &truncated);
  if (!ok)
  {
    sendError(request, 404, F("Path not found"));
    return;
  }

  String payload;
  payload.reserve(128 + accumulator.entries.length());
  payload += F("{\"path\":\"");
  appendJsonEscaped(payload, path.c_str());
  payload += F("\",\"parent\":\"");
  const String parent = buildParentPath(path);
  appendJsonEscaped(payload, parent.c_str());
  payload += F("\",\"ready\":true,\"busy\":");
  payload += SDManager::isBusy() ? F("true") : F("false");
  payload += F(",\"entryCount\":");
  payload += String(accumulator.count);
  payload += F(",\"truncated\":");
  payload += truncated ? F("true") : F("false");
  payload += F(",\"entries\":[");
  payload += accumulator.entries;
  payload += F("]}");

  sendJsonResponse(request, payload);
}

String sanitizeSdFilename(const String &raw)
{
  if (raw.length() == 0)
  {
    return String();
  }
  String trimmed = raw;
  trimmed.trim();
  if (trimmed.length() == 0)
  {
    return String();
  }
  if (trimmed.indexOf('/') >= 0 || trimmed.indexOf('\\') >= 0)
  {
    return String();
  }
  if (trimmed.indexOf("..") >= 0)
  {
    return String();
  }
  return trimmed;
}

String buildUploadTarget(const String &directory, const String &filename)
{
  String dir = sanitizeSdPath(directory);
  String name = sanitizeSdFilename(filename);
  if (dir.length() == 0 || name.length() == 0)
  {
    return String();
  }
  if (dir == "/")
  {
    return String("/") + name;
  }
  if (dir.endsWith("/"))
  {
    return dir + name;
  }
  return dir + "/" + name;
}

struct SdUploadContext
{
  File file;
  String target;
  bool failed = false;
  String error;
};

void handleSdUploadRequest(AsyncWebServerRequest *request)
{
  std::unique_ptr<SdUploadContext> ctx(static_cast<SdUploadContext *>(request->_tempObject));
  request->_tempObject = nullptr;

  if (!SDManager::isReady())
  {
    sendError(request, 503, F("SD not ready"));
    return;
  }
  if (!ctx)
  {
    sendError(request, 500, F("Upload context missing"));
    return;
  }
  if (ctx->failed)
  {
    sendError(request, 400, ctx->error.length() ? ctx->error : F("Upload failed"));
    return;
  }

  String payload = F("{\"status\":\"ok\",\"path\":\"");
  appendJsonEscaped(payload, ctx->target.c_str());
  payload += F("\"}");
  sendJsonResponse(request, payload);
}

void handleSdUploadData(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!SDManager::isReady())
  {
    return;
  }

  SdUploadContext *ctx = static_cast<SdUploadContext *>(request->_tempObject);
  if (!ctx)
  {
    ctx = new SdUploadContext();
    request->_tempObject = ctx;
  }

  if (ctx->failed)
  {
    if (final && ctx->file)
    {
      ctx->file.close();
      SDManager::setBusy(false);
    }
    return;
  }

  if (index == 0)
  {
    const String directory = request->hasParam("path", true) ? request->getParam("path", true)->value() : String("/");
    ctx->target = buildUploadTarget(directory, filename);
    if (ctx->target.length() == 0)
    {
      ctx->failed = true;
      ctx->error = F("Invalid upload path");
      return;
    }
    SDManager::setBusy(true);
    ctx->file = SD.open(ctx->target.c_str(), FILE_WRITE);
    if (!ctx->file)
    {
      ctx->failed = true;
      ctx->error = F("Cannot open target file");
      SDManager::setBusy(false);
      return;
    }
  }

  if (len > 0 && ctx->file)
  {
    if (ctx->file.write(data, len) != len)
    {
      ctx->failed = true;
      ctx->error = F("Write failed");
    }
  }

  if (final)
  {
    if (ctx->file)
    {
      ctx->file.close();
    }
    SDManager::setBusy(false);
  }
}

void handleSdDelete(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (!SDManager::isReady())
  {
    sendError(request, 503, F("SD not ready"));
    return;
  }

  JsonObjectConst obj = json.as<JsonObjectConst>();
  const String rawPath = obj["path"].as<String>();
  const String path = sanitizeSdPath(rawPath);
  if (path.length() == 0 || path == "/")
  {
    sendError(request, 400, F("Invalid path"));
    return;
  }

  if (!SDManager::instance().removePath(path.c_str()))
  {
    sendError(request, 404, F("Remove failed"));
    return;
  }

  sendJsonResponse(request, String(F("{\"status\":\"ok\"}")));
}

void attachLightStoreRoutes()
{
  server.on("/api/light/patterns", HTTP_GET, [](AsyncWebServerRequest *request) {
    ensureLightStoreReady();
    sendJsonResponse(request, WebLightStore::instance().buildPatternsJson());
  });

  server.on("/api/light/colors", HTTP_GET, [](AsyncWebServerRequest *request) {
    ensureLightStoreReady();
    sendJsonResponse(request, WebLightStore::instance().buildColorsJson());
  });

  auto *patternUpdate = new AsyncCallbackJsonWebHandler("/api/light/patterns");
  patternUpdate->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String affected;
    String error;
    JsonVariantConst body = json;
    if (!WebLightStore::instance().updatePattern(body, affected, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildPatternsJson(), "X-Light-Pattern", affected);
  });
  patternUpdate->setMethod(HTTP_POST);
  server.addHandler(patternUpdate);

  auto *patternDelete = new AsyncCallbackJsonWebHandler("/api/light/patterns/delete");
  patternDelete->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String affected;
    String error;
    JsonVariantConst body = json;
    if (!WebLightStore::instance().deletePattern(body, affected, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildPatternsJson(), "X-Light-Pattern", affected);
  });
  patternDelete->setMethod(HTTP_POST);
  server.addHandler(patternDelete);

  auto *patternSelect = new AsyncCallbackJsonWebHandler("/api/light/patterns/select");
  patternSelect->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String error;
    JsonObjectConst obj = json.as<JsonObjectConst>();
    String id = obj["id"].as<String>();
    if (!WebLightStore::instance().selectPattern(id, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildPatternsJson(), "X-Light-Pattern", WebLightStore::instance().getActivePatternId());
  });
  patternSelect->setMethod(HTTP_POST);
  server.addHandler(patternSelect);

  auto *colorUpdate = new AsyncCallbackJsonWebHandler("/api/light/colors");
  colorUpdate->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String affected;
    String error;
    JsonVariantConst body = json;
    if (!WebLightStore::instance().updateColor(body, affected, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildColorsJson(), "X-Light-Color", affected);
  });
  colorUpdate->setMethod(HTTP_POST);
  server.addHandler(colorUpdate);

  auto *colorDelete = new AsyncCallbackJsonWebHandler("/api/light/colors/delete");
  colorDelete->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String affected;
    String error;
    JsonVariantConst body = json;
    if (!WebLightStore::instance().deleteColor(body, affected, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildColorsJson(), "X-Light-Color", affected);
  });
  colorDelete->setMethod(HTTP_POST);
  server.addHandler(colorDelete);

  auto *colorSelect = new AsyncCallbackJsonWebHandler("/api/light/colors/select");
  colorSelect->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String error;
    JsonObjectConst obj = json.as<JsonObjectConst>();
    String id = obj["id"].as<String>();
    if (!WebLightStore::instance().selectColor(id, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, WebLightStore::instance().buildColorsJson(), "X-Light-Color", WebLightStore::instance().getActiveColorId());
  });
  colorSelect->setMethod(HTTP_POST);
  server.addHandler(colorSelect);

  auto *previewHandler = new AsyncCallbackJsonWebHandler("/api/light/preview");
  previewHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureLightStoreReady();
    String error;
    JsonVariantConst body = json;
    if (!WebLightStore::instance().preview(body, error))
    {
      sendError(request, 400, error);
      return;
    }
    sendJsonResponse(request, String("{\"status\":\"ok\"}"));
  });
  previewHandler->setMethod(HTTP_POST);
  server.addHandler(previewHandler);

  auto *sdDeleteHandler = new AsyncCallbackJsonWebHandler("/api/sd/delete");
  sdDeleteHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    handleSdDelete(request, json);
  });
  sdDeleteHandler->setMethod(HTTP_POST);
  server.addHandler(sdDeleteHandler);
}

} // namespace

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
  ensureLightStoreReady();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/setBrightness", HTTP_GET, handleSetBrightness);
  server.on("/getBrightness", HTTP_GET, handleGetBrightness);
  server.on("/setWebAudioLevel", HTTP_GET, handleSetWebAudioLevel);
  server.on("/getWebAudioLevel", HTTP_GET, handleGetWebAudioLevel);
  server.on("/light/next", HTTP_GET, handleLightNext);
  server.on("/api/sd/status", HTTP_GET, sendSdStatus);
  server.on("/api/sd/list", HTTP_GET, sendSdList);
  server.on("/api/sd/upload", HTTP_POST, handleSdUploadRequest, handleSdUploadData);

  // Serve the monolithic UI assets directly from the SD card so the browser can load styling and logic.
  server.serveStatic("/styles.css", SD, "/styles.css");
  server.serveStatic("/kwal.js", SD, "/kwal.js");

  attachLightStoreRoutes();

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
