#include "AuthManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <mbedtls/md.h>   // generic message-digest API, stable across ESP-IDF versions

// ── Public ────────────────────────────────────────────────────────────────────

bool AuthManager::begin() {
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("[Auth] No auth config – writing default password 'admin'");
    return setPassword(DEFAULT_PASSWORD);
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[Auth] Cannot open auth config – resetting to default");
    return setPassword(DEFAULT_PASSWORD);
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, f);
  f.close();

  if (err || !doc["hash"].is<const char*>()) {
    Serial.println("[Auth] Bad auth config – resetting to default");
    return setPassword(DEFAULT_PASSWORD);
  }

  _hash = doc["hash"].as<String>();
  Serial.println("[Auth] Password loaded from config");
  return true;
}

bool AuthManager::checkPassword(const String& password) const {
  return sha256hex(password) == _hash;
}

bool AuthManager::setPassword(const String& newPassword) {
  _hash = sha256hex(newPassword);

  JsonDocument doc;
  doc["hash"] = _hash;

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("[Auth] Cannot write auth config");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("[Auth] Password updated and saved");
  return true;
}

String AuthManager::createSession() {
  pruneExpiredSessions();
  if (_sessions.size() >= MAX_SESSIONS) {
    Serial.println("[Auth] Session table full – clearing all sessions");
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

  _sessions[token] = millis();
  Serial.printf("[Auth] Session created (active: %zu)\n", _sessions.size());
  return token;
}

bool AuthManager::validateSession(const String& token) const {
  auto it = _sessions.find(token);
  if (it == _sessions.end()) return false;
  // unsigned subtraction wraps correctly, handles millis() rollover after ~49 days
  return (millis() - it->second) < SESSION_DURATION_MS;
}

void AuthManager::destroySession(const String& token) {
  if (_sessions.erase(token)) {
    Serial.println("[Auth] Session destroyed (logout)");
  }
}

// ── Private ───────────────────────────────────────────────────────────────────

String AuthManager::sha256hex(const String& input) {
  uint8_t digest[32];

  // Use the generic mbedTLS MD API – compatible across ESP-IDF 4.x / 5.x
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
    if (now - it->second >= SESSION_DURATION_MS) {
      it = _sessions.erase(it);
    } else {
      ++it;
    }
  }
}
