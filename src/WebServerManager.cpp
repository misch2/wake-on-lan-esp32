#include "WebServerManager.h"

#include <ArduinoJson.h>

#include "Config.h"
#include "HtmlContent.h"
#include "WakeOnLan.h"

WebServerManager::WebServerManager(DeviceManager& deviceManager)
    : _server(WEB_SERVER_PORT), _deviceManager(deviceManager) {}

void WebServerManager::setOnWakeCallback(std::function<void(const char* alias)> cb) {
  _onWakeCallback = cb;
}

void WebServerManager::setGetOnlineStatusCallback(std::function<bool(size_t index)> cb) {
  _getOnlineStatus = cb;
}

void WebServerManager::setOnDeviceListChanged(std::function<void()> cb) {
  _onDeviceListChanged = cb;
}

void WebServerManager::begin() {
  registerRoutes();
  _server.begin();
  Serial.printf("[HTTP] Web server started on port %d\n", WEB_SERVER_PORT);
}

void WebServerManager::registerRoutes() {
  // ── GET / ─────────────────────────────────────────────────────────────────
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", HTML_CONTENT);
  });

  // ── GET /admin ────────────────────────────────────────────────────────────
  _server.on("/admin", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", HTML_ADMIN);
  });

  // ── GET /api/devices ──────────────────────────────────────────────────────
  _server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
    const auto& devices = _deviceManager.devices();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < devices.size(); i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["alias"]  = devices[i].alias;
      obj["mac"]    = devices[i].mac;
      obj["ip"]     = devices[i].ip;
      obj["online"] = _getOnlineStatus ? _getOnlineStatus(i) : false;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // ── POST /api/devices (body: {"alias":"…","mac":"…","ip":"…"}) ─────────────
  _server.on(
      "/api/devices", HTTP_POST,
      // onRequest – not used (body handler sends the response)
      [](AsyncWebServerRequest* request) {},
      // onUpload  – not used
      nullptr,
      // onBody – fires for each chunk; we process once the full body arrives
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
             size_t index, size_t total) {
        if (index + len < total) return;  // wait for the last (or only) chunk

        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
          request->send(400, "application/json", R"({"error":"Invalid JSON"})");
          return;
        }

        const String alias = doc["alias"] | "";
        const String mac   = doc["mac"]   | "";
        const String ip    = doc["ip"]    | "";

        if (alias.isEmpty() || mac.isEmpty()) {
          request->send(400, "application/json",
                        R"({"error":"Fields 'alias' and 'mac' are required"})");
          return;
        }

        if (!_deviceManager.addDevice(alias, mac, ip)) {
          request->send(409, "application/json",
                        R"({"error":"A device with that MAC already exists"})");
          return;
        }

        _deviceManager.save();
        if (_onDeviceListChanged) _onDeviceListChanged();
        request->send(200, "application/json", R"({"status":"ok"})");
      });

  // ── DELETE /api/devices?mac=XX:XX:XX:XX:XX:XX ────────────────────────────
  _server.on("/api/devices", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("mac")) {
      request->send(400, "application/json", R"({"error":"Missing 'mac' query parameter"})");
      return;
    }

    const String mac = request->getParam("mac")->value();

    if (!_deviceManager.removeDevice(mac)) {
      request->send(404, "application/json", R"({"error":"Device not found"})");
      return;
    }

    _deviceManager.save();
    if (_onDeviceListChanged) _onDeviceListChanged();
    request->send(200, "application/json", R"({"status":"ok"})");
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
      for (const auto& d : _deviceManager.devices()) {
        if (d.mac.equalsIgnoreCase(mac)) {
          alias = d.alias.c_str();
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
  _server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
  });
}

