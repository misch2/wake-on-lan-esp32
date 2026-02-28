#include "AuthManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <mbedtls/md.h>   // generic message-digest API, stable across ESP-IDF versions

// ── Public ────────────────────────────────────────────────────────────────────

bool AuthManager::begin() {
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("[Auth] No auth config - creating default admin account");
    _users[DEFAULT_USERNAME] = sha256hex(DEFAULT_PASSWORD);
    return save();
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[Auth] Cannot open auth config - resetting to default");
    _users[DEFAULT_USERNAME] = sha256hex(DEFAULT_PASSWORD);
    return save();
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.println("[Auth] Bad auth config - resetting to default");
    _users[DEFAULT_USERNAME] = sha256hex(DEFAULT_PASSWORD);
    return save();
  }

  // Migrate legacy single-user format: {"hash":"..."}
  if (doc["hash"].is<const char*>() && !doc["users"].is<JsonArray>()) {
    Serial.println("[Auth] Migrating legacy auth config to multi-user format");
    _users.clear();
    _users[DEFAULT_USERNAME] = doc["hash"].as<String>();
    return save();
  }

  // Current multi-user format: {"users":[{"username":"...","hash":"..."}]}
  if (!doc["users"].is<JsonArray>()) {
    Serial.println("[Auth] Unknown auth config format - resetting to default");
    _users[DEFAULT_USERNAME] = sha256hex(DEFAULT_PASSWORD);
    return save();
  }

  _users.clear();
  for (JsonObject u : doc["users"].as<JsonArray>()) {
    const String uname = u["username"] | "";
    const String hash  = u["hash"]     | "";
    if (!uname.isEmpty() && !hash.isEmpty()) {
      _users[uname] = hash;
    }
  }

  if (_users.empty()) {
    Serial.println("[Auth] Auth config has no valid users - resetting to default");
    _users[DEFAULT_USERNAME] = sha256hex(DEFAULT_PASSWORD);
    return save();
  }

  Serial.printf("[Auth] Loaded %zu admin account(s) from config\n", _users.size());
  return true;
}

bool AuthManager::checkCredentials(const String& username, const String& password) const {
  const auto it = _users.find(username);
  if (it == _users.end()) return false;
  return sha256hex(password) == it->second;
}

String AuthManager::createSession(const String& username) {
  pruneExpiredSessions();
  if (_sessions.size() >= MAX_SESSIONS) {
    Serial.println("[Auth] Session table full - clearing all sessions");
    _sessions.clear();
  }

  // 128-bit token as 32 lowercase hex characters
  String token;
  token.reserve(32);
  for (int i = 0; i < 4; i++) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)esp_random());
    token += buf;
  }

  _sessions[token] = {millis(), username};
  Serial.printf("[Auth] Session created for '%s' (active: %zu)\n", username.c_str(), _sessions.size());
  return token;
}

bool AuthManager::validateSession(const String& token) const {
  const auto it = _sessions.find(token);
  if (it == _sessions.end()) return false;
  return (millis() - it->second.createdAt) < SESSION_DURATION_MS;
}

String AuthManager::getSessionUser(const String& token) const {
  const auto it = _sessions.find(token);
  if (it == _sessions.end()) return "";
  if ((millis() - it->second.createdAt) >= SESSION_DURATION_MS) return "";
  return it->second.username;
}

void AuthManager::destroySession(const String& token) {
  if (_sessions.erase(token)) {
    Serial.println("[Auth] Session destroyed (logout)");
  }
}

std::vector<String> AuthManager::listUsers() const {
  std::vector<String> out;
  out.reserve(_users.size());
  for (const auto& kv : _users) {
    out.push_back(kv.first);
  }
  return out;  // std::map iterates in sorted order
}

bool AuthManager::addUser(const String& username, const String& password) {
  if (username.isEmpty()) return false;
  if (_users.count(username)) {
    Serial.printf("[Auth] addUser: '%s' already exists\n", username.c_str());
    return false;
  }
  _users[username] = sha256hex(password);
  Serial.printf("[Auth] User '%s' added\n", username.c_str());
  return save();
}

bool AuthManager::removeUser(const String& username) {
  if (_users.size() <= 1) {
    Serial.println("[Auth] removeUser: cannot remove the last admin account");
    return false;
  }
  if (!_users.erase(username)) {
    Serial.printf("[Auth] removeUser: '%s' not found\n", username.c_str());
    return false;
  }
  // Invalidate all active sessions belonging to the removed user
  for (auto it = _sessions.begin(); it != _sessions.end(); ) {
    if (it->second.username == username) {
      it = _sessions.erase(it);
    } else {
      ++it;
    }
  }
  Serial.printf("[Auth] User '%s' removed\n", username.c_str());
  return save();
}

bool AuthManager::changePassword(const String& username, const String& newPassword) {
  const auto it = _users.find(username);
  if (it == _users.end()) {
    Serial.printf("[Auth] changePassword: user '%s' not found\n", username.c_str());
    return false;
  }
  it->second = sha256hex(newPassword);
  Serial.printf("[Auth] Password changed for '%s'\n", username.c_str());
  return save();
}

// ── Private ───────────────────────────────────────────────────────────────────

bool AuthManager::save() const {
  JsonDocument doc;
  JsonArray arr = doc["users"].to<JsonArray>();
  for (const auto& kv : _users) {
    JsonObject u = arr.add<JsonObject>();
    u["username"] = kv.first;
    u["hash"]     = kv.second;
  }

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("[Auth] Cannot write auth config");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("[Auth] Auth config saved");
  return true;
}

String AuthManager::sha256hex(const String& input) {
  uint8_t digest[32];

  // Use the generic mbedTLS MD API - compatible across ESP-IDF 4.x / 5.x
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md(info,
             reinterpret_cast<const uint8_t*>(input.c_str()),
             input.length(),
             digest);

  String hex;
  hex.reserve(64);
  for (int i = 0; i < 32; i++) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", digest[i]);
    hex += buf;
  }
  return hex;
}

void AuthManager::pruneExpiredSessions() {
  const unsigned long now = millis();
  for (auto it = _sessions.begin(); it != _sessions.end(); ) {
    if (now - it->second.createdAt >= SESSION_DURATION_MS) {
      it = _sessions.erase(it);
    } else {
      ++it;
    }
  }
}
