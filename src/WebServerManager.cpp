#include "WebServerManager.h"

#include <ArduinoJson.h>
#include <cstring>

#include "Config.h"
#include "HtmlContent.h"
#include "WakeOnLan.h"

WebServerManager::WebServerManager() : _server(WEB_SERVER_PORT) {}

void WebServerManager::setOnWakeCallback(std::function<void(const char* alias)> cb) {
  _onWakeCallback = cb;
}

void WebServerManager::setGetOnlineStatusCallback(std::function<bool(size_t index)> cb) {
  _getOnlineStatus = cb;
}

void WebServerManager::begin() {
  registerRoutes();
  _server.begin();
  Serial.printf("[HTTP] Web server started on port %d\n", WEB_SERVER_PORT);
}

void WebServerManager::registerRoutes() {
  // ── GET / ─────────────────────────────────────────────────────────────────
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/html", HTML_CONTENT); });

  // ── GET /api/devices ──────────────────────────────────────────────────────
  _server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < DEVICE_COUNT; i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["alias"]  = DEVICES[i].alias;
      obj["mac"]    = DEVICES[i].mac;
      obj["ip"]     = DEVICES[i].ip;
      obj["online"] = _getOnlineStatus ? _getOnlineStatus(i) : false;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // ── POST /api/wake?mac=XX:XX:XX:XX:XX:XX ─────────────────────────────────
  _server.on("/api/wake", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("mac")) {
      request->send(400, "application/json", R"({"error":"Missing 'mac' query parameter"})");
      return;
    }

    const String mac = request->getParam("mac")->value();

    if (WakeOnLan::wake(mac.c_str())) {
      // Resolve alias for the display callback
      const char* alias = mac.c_str();
      for (size_t i = 0; i < DEVICE_COUNT; i++) {
        if (strcasecmp(DEVICES[i].mac, mac.c_str()) == 0) {
          alias = DEVICES[i].alias;
          break;
        }
      }
      if (_onWakeCallback) _onWakeCallback(alias);
      request->send(200, "application/json", R"({"status":"ok"})");
    } else {
      request->send(400, "application/json", R"({"error":"Invalid MAC address format"})");
    }
  });

  // ── 404 ───────────────────────────────────────────────────────────────────
  _server.onNotFound([](AsyncWebServerRequest* request) { request->send(404, "text/plain", "Not Found"); });
}
