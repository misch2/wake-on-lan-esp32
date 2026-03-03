#include "WebServerManager.h"

#include <ArduinoJson.h>

#include "Config.h"
#include "HtmlContent.h"
#include "WakeOnLan.h"

WebServerManager::WebServerManager(DeviceManager& deviceManager, AuthManager& authManager)
    : _server(WEB_SERVER_PORT), _deviceManager(deviceManager), _authManager(authManager) {}

void WebServerManager::setOnWakeCallback(std::function<void(const char* alias)> cb) { _onWakeCallback = cb; }

void WebServerManager::setGetOnlineStatusCallback(std::function<bool(size_t index)> cb) { _getOnlineStatus = cb; }

void WebServerManager::setOnDeviceListChanged(std::function<void()> cb) { _onDeviceListChanged = cb; }

void WebServerManager::begin() {
  registerRoutes();
  _server.begin();
  Serial.printf("[HTTP] Web server started on port %d\n", WEB_SERVER_PORT);
}

// Private helpers

String WebServerManager::extractToken(AsyncWebServerRequest* request) const {
  const AsyncWebHeader* h = request->getHeader("Cookie");
  if (!h) return "";
  const String& cookies = h->value();
  const int idx = cookies.indexOf("wol_session=");
  if (idx < 0) return "";
  String token = cookies.substring(idx + 12);
  const int end = token.indexOf(';');
  if (end >= 0) token = token.substring(0, end);
  token.trim();
  return token;
}

bool WebServerManager::isAuthenticated(AsyncWebServerRequest* request) const { return _authManager.validateSession(extractToken(request)); }

static void redirectToLogin(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* resp = request->beginResponse(302);
  resp->addHeader("Location", "/login?next=" + request->url());
  request->send(resp);
}

// Routes

void WebServerManager::registerRoutes() {
  // GET /
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = HTML_CONTENT;
    html.replace("{{GIT_HASH}}", GIT_HASH);
    request->send(200, "text/html", html);
  });

  // GET /login
  _server.on("/login", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/html", HTML_LOGIN); });

  // POST /api/login
  _server.on(
      "/api/login", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index + len < total) return;

        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
          request->send(400, "application/json", R"({"error":"Invalid JSON"})");
          return;
        }

        const String username = doc["username"] | "";
        const String password = doc["password"] | "";

        if (!_authManager.checkCredentials(username, password)) {
          request->send(401, "application/json", R"({"error":"Invalid username or password"})");
          return;
        }

        const String token = _authManager.createSession(username);
        AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", R"({"status":"ok"})");
        resp->addHeader("Set-Cookie", "wol_session=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=86400");
        request->send(resp);
      });

  // POST /api/logout
  _server.on("/api/logout", HTTP_POST, [this](AsyncWebServerRequest* request) {
    _authManager.destroySession(extractToken(request));
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", R"({"status":"ok"})");
    resp->addHeader("Set-Cookie", "wol_session=; Path=/; HttpOnly; Max-Age=0");
    request->send(resp);
  });

  // GET /admin (protected)
  _server.on("/admin", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
      redirectToLogin(request);
      return;
    }
    request->send(200, "text/html", HTML_ADMIN);
  });

  // GET /api/devices (public)
  _server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
    const auto& devices = _deviceManager.devices();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < devices.size(); i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["alias"] = devices[i].alias;
      obj["mac"] = devices[i].mac;
      obj["ip"] = devices[i].ip;
      obj["online"] = _getOnlineStatus ? _getOnlineStatus(i) : false;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // POST /api/devices (protected)
  _server.on(
      "/api/devices", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index + len < total) return;

        if (!isAuthenticated(request)) {
          request->send(401, "application/json", R"({"error":"Unauthorized"})");
          return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
          request->send(400, "application/json", R"({"error":"Invalid JSON"})");
          return;
        }

        const String alias = doc["alias"] | "";
        const String mac = doc["mac"] | "";
        const String ip = doc["ip"] | "";

        if (alias.isEmpty() || mac.isEmpty()) {
          request->send(400, "application/json", R"({"error":"Fields 'alias' and 'mac' are required"})");
          return;
        }

        if (!_deviceManager.addDevice(alias, mac, ip)) {
          request->send(409, "application/json", R"({"error":"A device with that MAC already exists"})");
          return;
        }

        _deviceManager.save();
        if (_onDeviceListChanged) _onDeviceListChanged();
        request->send(200, "application/json", R"({"status":"ok"})");
      });

  // DELETE /api/devices (protected)
  _server.on("/api/devices", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
      request->send(401, "application/json", R"({"error":"Unauthorized"})");
      return;
    }

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

  // POST /api/password (protected)
  _server.on(
      "/api/password", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index + len < total) return;

        const String token = extractToken(request);
        if (!_authManager.validateSession(token)) {
          request->send(401, "application/json", R"({"error":"Unauthorized"})");
          return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
          request->send(400, "application/json", R"({"error":"Invalid JSON"})");
          return;
        }

        const String current = doc["current"] | "";
        const String newPassword = doc["newPassword"] | "";
        const String username = _authManager.getSessionUser(token);

        if (!_authManager.checkCredentials(username, current)) {
          request->send(403, "application/json", R"({"error":"Current password is incorrect"})");
          return;
        }

        if (newPassword.length() < 4) {
          request->send(400, "application/json", R"({"error":"New password must be at least 4 characters"})");
          return;
        }

        if (!_authManager.changePassword(username, newPassword)) {
          request->send(500, "application/json", R"({"error":"Failed to save password"})");
          return;
        }

        request->send(200, "application/json", R"({"status":"ok"})");
      });

  // GET /api/users (protected)
  _server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
      request->send(401, "application/json", R"({"error":"Unauthorized"})");
      return;
    }
    const auto users = _authManager.listUsers();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& u : users) arr.add(u);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // POST /api/users (protected)
  _server.on(
      "/api/users", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index + len < total) return;

        if (!isAuthenticated(request)) {
          request->send(401, "application/json", R"({"error":"Unauthorized"})");
          return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
          request->send(400, "application/json", R"({"error":"Invalid JSON"})");
          return;
        }

        const String username = doc["username"] | "";
        const String password = doc["password"] | "";

        if (username.isEmpty() || password.isEmpty()) {
          request->send(400, "application/json", R"({"error":"Fields 'username' and 'password' are required"})");
          return;
        }

        if (password.length() < 4) {
          request->send(400, "application/json", R"({"error":"Password must be at least 4 characters"})");
          return;
        }

        if (!_authManager.addUser(username, password)) {
          request->send(409, "application/json", R"({"error":"Username already exists"})");
          return;
        }

        request->send(200, "application/json", R"({"status":"ok"})");
      });

  // DELETE /api/users (protected)
  _server.on("/api/users", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
      request->send(401, "application/json", R"({"error":"Unauthorized"})");
      return;
    }

    if (!request->hasParam("username")) {
      request->send(400, "application/json", R"({"error":"Missing 'username' query parameter"})");
      return;
    }

    const String username = request->getParam("username")->value();

    const String self = _authManager.getSessionUser(extractToken(request));
    if (username == self) {
      request->send(400, "application/json", R"({"error":"Cannot delete your own account"})");
      return;
    }

    if (!_authManager.removeUser(username)) {
      request->send(400, "application/json", R"({"error":"Cannot remove user: not found or last account"})");
      return;
    }

    request->send(200, "application/json", R"({"status":"ok"})");
  });

  // POST /api/wake (public)
  _server.on("/api/wake", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("mac")) {
      request->send(400, "application/json", R"({"error":"Missing 'mac' query parameter"})");
      return;
    }

    const String mac = request->getParam("mac")->value();

    if (WakeOnLan::wake(mac.c_str())) {
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

  _server.onNotFound([](AsyncWebServerRequest* request) { request->send(404, "text/plain", "Not Found"); });
}
