#include "WebInterfaceManager.h"
#include "Globals.h"
#include "LightManager.h"
#include "SDManager.h"
#include "WiFiManager.h"
#include "ConductManager.h"
#include "AudioManager.h"
#include "SDVoting.h"
#include "ColorsStore.h"
#include "../ConductManager20251030/Web/WebDirector.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <WiFi.h>
#include <memory>
#include <cstring>

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

void ensureColorsStoreReady()
{
  if (!ColorsStore::instance().isReady())
  {
    ColorsStore::instance().begin();
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

namespace {

class SdBusyLock {
public:
  SdBusyLock()
  {
    acquired_ = !SDManager::isBusy();
    if (acquired_)
    {
      SDManager::setBusy(true);
    }
  }

  ~SdBusyLock()
  {
    release();
  }

  bool acquired() const { return acquired_; }

  void release()
  {
    if (acquired_)
    {
      SDManager::setBusy(false);
      acquired_ = false;
    }
  }

private:
  bool acquired_{false};
};

String parentPath(const String &path)
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
  return path.substring(0, lastSlash);
}

String extractBaseName(const char *fullPath)
{
  if (!fullPath)
  {
    return String();
  }
  const char *slash = strrchr(fullPath, '/');
  if (slash && slash[1] != '\0')
  {
    return String(slash + 1);
  }
  return String(fullPath);
}

bool removeSdPath(const String &targetPath, String &errorMessage)
{
  File node = SD.open(targetPath.c_str(), FILE_READ);
  if (!node)
  {
    errorMessage = F("Path not found");
    return false;
  }
  const bool isDir = node.isDirectory();
  node.close();

  if (!isDir)
  {
    if (!SD.remove(targetPath.c_str()))
    {
      errorMessage = F("Delete failed");
      return false;
    }
    return true;
  }

  File dir = SD.open(targetPath.c_str(), FILE_READ);
  if (!dir)
  {
    errorMessage = F("Open directory failed");
    return false;
  }
  for (File child = dir.openNextFile(); child; child = dir.openNextFile())
  {
    String childPath = targetPath;
    if (!childPath.endsWith("/"))
    {
      childPath += '/';
    }
    childPath += extractBaseName(child.name());
    child.close();
    if (!removeSdPath(childPath, errorMessage))
    {
      dir.close();
      return false;
    }
  }
  dir.close();
  if (!SD.rmdir(targetPath.c_str()))
  {
    errorMessage = F("Remove directory failed");
    return false;
  }
  return true;
}

} // namespace

void handleSdList(AsyncWebServerRequest *request)
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

  SdBusyLock lock;
  if (!lock.acquired())
  {
    sendError(request, 503, F("SD busy"));
    return;
  }

  File dir = SD.open(path.c_str(), FILE_READ);
  if (!dir)
  {
    sendError(request, 404, F("Path not found"));
    return;
  }
  if (!dir.isDirectory())
  {
    dir.close();
    sendError(request, 400, F("Not a directory"));
    return;
  }

  constexpr size_t kMaxEntries = 256U;
  size_t entryCount = 0;
  bool truncated = false;
  String entries;
  entries.reserve(512);

  for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile())
  {
    if (entryCount > 0)
    {
      entries += ',';
    }
    const bool isDir = entry.isDirectory();
    const uint32_t sizeBytes = isDir ? 0U : static_cast<uint32_t>(entry.size());
    const String baseName = extractBaseName(entry.name());

    entries += F("{\"name\":\"");
    appendJsonEscaped(entries, baseName.c_str());
    entries += F("\",\"type\":\"");
    entries += isDir ? F("dir") : F("file");
    entries += F("\",\"size\":");
    entries += String(sizeBytes);
    entries += '}';

    entry.close();
    ++entryCount;
    if (entryCount >= kMaxEntries)
    {
      truncated = true;
      break;
    }
  }
  dir.close();

  lock.release();

  const bool busyFlag = SDManager::isBusy();
  String payload;
  payload.reserve(entries.length() + 160);
  payload += F("{\"path\":\"");
  appendJsonEscaped(payload, path.c_str());
  payload += F("\",\"parent\":\"");
  appendJsonEscaped(payload, parentPath(path).c_str());
  payload += F("\",\"ready\":true,\"busy\":");
  payload += busyFlag ? F("true") : F("false");
  payload += F(",\"entryCount\":");
  payload += String(entryCount);
  payload += F(",\"truncated\":");
  payload += truncated ? F("true") : F("false");
  payload += F(",\"entries\":[");
  payload += entries;
  payload += F("]}");

  sendJsonResponse(request, payload);
}

void handlePatternsList(AsyncWebServerRequest *request)
{
  ensureColorsStoreReady();
  ColorsStore &store = ColorsStore::instance();
  String payload = store.buildPatternsJson();
  if (payload.isEmpty())
  {
    sendError(request, 500, F("Pattern export failed"));
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
  response->addHeader("Cache-Control", "no-store");
  const String activeId = store.getActivePatternId();
  if (!activeId.isEmpty())
  {
    response->addHeader("X-Pattern", activeId);
  }
  request->send(response);
}

void handleColorsList(AsyncWebServerRequest *request)
{
  ensureColorsStoreReady();
  ColorsStore &store = ColorsStore::instance();
  String payload = store.buildColorsJson();
  if (payload.isEmpty())
  {
    sendError(request, 500, F("Color export failed"));
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
  response->addHeader("Cache-Control", "no-store");
  const String activeId = store.getActiveColorId();
  if (!activeId.isEmpty())
  {
    response->addHeader("X-Color", activeId);
  }
  request->send(response);
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

  if (!json.is<JsonObject>())
  {
    sendError(request, 400, F("Invalid payload"));
    return;
  }

  JsonObjectConst obj = json.as<JsonObjectConst>();
  if (obj.isNull())
  {
    sendError(request, 400, F("Invalid payload"));
    return;
  }

  const String rawPath = obj["path"].as<String>();
  const String path = sanitizeSdPath(rawPath);
  if (path.length() == 0 || path == "/")
  {
    sendError(request, 400, F("Invalid path"));
    return;
  }

  SdBusyLock lock;
  if (!lock.acquired())
  {
    sendError(request, 503, F("SD busy"));
    return;
  }

  String errorMessage;
  if (!removeSdPath(path, errorMessage))
  {
    if (errorMessage.isEmpty())
    {
      errorMessage = F("Delete failed");
    }
    sendError(request, 400, errorMessage);
    return;
  }

  sendJsonResponse(request, F("{\"status\":\"ok\"}"));
}

void attachPatternColorRoutes()
{
  server.on("/api/patterns", HTTP_GET, handlePatternsList);
  server.on("/api/colors", HTTP_GET, handleColorsList);

  auto *patternSelect = new AsyncCallbackJsonWebHandler("/api/patterns/select");
  patternSelect->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    String error;
    String id;
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (!obj.isNull() && obj.containsKey("id"))
    {
      id = obj["id"].as<String>();
    }
    if (id.isEmpty())
    {
      if (request->hasParam("id", true))
      {
        id = request->getParam("id", true)->value();
      }
      else if (request->hasParam("id"))
      {
        id = request->getParam("id")->value();
      }
    }
    PF("[ColorsStore] HTTP pattern/select id='%s' content-type='%s'\n",
       id.c_str(),
       request->contentType().c_str());
    if (!ColorsStore::instance().selectPattern(id, error))
    {
      sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
      return;
    }
    const String activeId = ColorsStore::instance().getActivePatternId();
    sendJsonResponse(request, ColorsStore::instance().buildPatternsJson(), "X-Pattern", activeId);
  });
  patternSelect->setMethod(HTTP_POST);
  server.addHandler(patternSelect);

  auto *patternDelete = new AsyncCallbackJsonWebHandler("/api/patterns/delete");
  patternDelete->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (obj.isNull())
    {
      sendError(request, 400, F("invalid payload"));
      return;
    }
    String affected;
    String error;
    if (!ColorsStore::instance().deletePattern(obj, affected, error))
    {
      sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
      return;
    }
    String payload = ColorsStore::instance().buildPatternsJson();
    if (payload.isEmpty())
    {
      sendError(request, 500, F("pattern export failed"));
      return;
    }
    const String headerId = affected.length() ? affected : ColorsStore::instance().getActivePatternId();
    sendJsonResponse(request, payload, "X-Pattern", headerId);
  });
  patternDelete->setMethod(HTTP_POST);
  server.addHandler(patternDelete);

  auto *patternUpdate = new AsyncCallbackJsonWebHandler("/api/patterns");
  patternUpdate->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (obj.isNull())
    {
      sendError(request, 400, F("invalid payload"));
      return;
    }
    PF("[PatternStore] HTTP pattern/update content-type='%s' length=%d\n",
       request->contentType().c_str(),
       static_cast<int>(request->contentLength()));
    ColorsStore &store = ColorsStore::instance();
    String affected;
    String errorMessage;
    if (!store.updatePattern(obj, affected, errorMessage))
    {
      sendError(request, 400, errorMessage.length() ? errorMessage : F("update failed"));
      return;
    }
    String payload = store.buildPatternsJson();
    if (payload.isEmpty())
    {
      sendError(request, 500, F("pattern export failed"));
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    const String activeId = affected.length() ? affected : store.getActivePatternId();
    if (!activeId.isEmpty())
    {
      response->addHeader("X-Pattern", activeId);
    }
    request->send(response);
  });
  patternUpdate->setMethod(HTTP_POST);
  server.addHandler(patternUpdate);

  auto *colorSelect = new AsyncCallbackJsonWebHandler("/api/colors/select");
  colorSelect->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    String error;
    String id;
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (!obj.isNull() && obj.containsKey("id"))
    {
      id = obj["id"].as<String>();
    }
    if (id.isEmpty())
    {
      if (request->hasParam("id", true))
      {
        id = request->getParam("id", true)->value();
      }
      else if (request->hasParam("id"))
      {
        id = request->getParam("id")->value();
      }
    }
    PF("[ColorsStore] HTTP color/select id='%s' content-type='%s'\n",
       id.c_str(),
       request->contentType().c_str());
    if (!ColorsStore::instance().selectColor(id, error))
    {
      sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
      return;
    }
    const String activeId = ColorsStore::instance().getActiveColorId();
    sendJsonResponse(request, ColorsStore::instance().buildColorsJson(), "X-Color", activeId);
  });
  colorSelect->setMethod(HTTP_POST);
  server.addHandler(colorSelect);

  auto *colorDelete = new AsyncCallbackJsonWebHandler("/api/colors/delete");
  colorDelete->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (obj.isNull())
    {
      sendError(request, 400, F("invalid payload"));
      return;
    }
    String affected;
    String error;
    if (!ColorsStore::instance().deleteColor(obj, affected, error))
    {
      sendError(request, 400, error.isEmpty() ? F("invalid payload") : error);
      return;
    }
    String payload = ColorsStore::instance().buildColorsJson();
    if (payload.isEmpty())
    {
      sendError(request, 500, F("color export failed"));
      return;
    }
    const String headerId = affected.length() ? affected : ColorsStore::instance().getActiveColorId();
    sendJsonResponse(request, payload, "X-Color", headerId);
  });
  colorDelete->setMethod(HTTP_POST);
  server.addHandler(colorDelete);

  auto *colorUpdate = new AsyncCallbackJsonWebHandler("/api/colors");
  colorUpdate->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    JsonObjectConst obj = json.as<JsonObjectConst>();
    if (obj.isNull())
    {
      sendError(request, 400, F("invalid payload"));
      return;
    }
    PF("[ColorsStore] HTTP colors/update content-type='%s' length=%d\n",
       request->contentType().c_str(),
       static_cast<int>(request->contentLength()));
    ColorsStore &store = ColorsStore::instance();
    String affected;
    String errorMessage;
    if (!store.updateColor(obj, affected, errorMessage))
    {
      sendError(request, 400, errorMessage.length() ? errorMessage : F("update failed"));
      return;
    }
    String payload = store.buildColorsJson();
    if (payload.isEmpty())
    {
      sendError(request, 500, F("color export failed"));
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    const String activeId = affected.length() ? affected : store.getActiveColorId();
    if (!activeId.isEmpty())
    {
      response->addHeader("X-Color", activeId);
    }
    request->send(response);
  });
  colorUpdate->setMethod(HTTP_POST);
  server.addHandler(colorUpdate);

  auto *previewHandler = new AsyncCallbackJsonWebHandler("/api/patterns/preview", nullptr, 4096);
  previewHandler->setMaxContentLength(2048);
  previewHandler->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
    ensureColorsStoreReady();
    String error;
    JsonVariantConst body = json;
    if (!ColorsStore::instance().preview(body, error))
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

/* ----- setup/loop glue ----- */

void beginWebInterface()
{
  ensureColorsStoreReady();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/setBrightness", HTTP_GET, handleSetBrightness);
  server.on("/getBrightness", HTTP_GET, handleGetBrightness);
  server.on("/setWebAudioLevel", HTTP_GET, handleSetWebAudioLevel);
  server.on("/getWebAudioLevel", HTTP_GET, handleGetWebAudioLevel);
  server.on("/api/sd/status", HTTP_GET, sendSdStatus);
  server.on("/api/sd/list", HTTP_GET, handleSdList);
  server.on("/api/sd/upload", HTTP_POST, handleSdUploadRequest, handleSdUploadData);

  // Serve the monolithic UI assets directly from the SD card so the browser can load styling and logic.
  server.serveStatic("/styles.css", SD, "/styles.css");
  server.serveStatic("/kwal.js", SD, "/kwal.js");

  attachPatternColorRoutes();

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
