#include "AuthManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <mbedtls/md.h>

// Public

static void makeDefaultUser(std::map<String, AuthManager::UserRecord>& users) {
  const String salt = AuthManager::generateSalt();
  users[AuthManager::DEFAULT_USERNAME] = {AuthManager::hashPassword(salt, AuthManager::DEFAULT_PASSWORD), salt};
}

bool AuthManager::begin() {
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("[Auth] No auth config - creating default admin account");
    makeDefaultUser(_users);
    return save();
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[Auth] Cannot open auth config - resetting to default");
    makeDefaultUser(_users);
    return save();
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.println("[Auth] Bad auth config - resetting to default");
    makeDefaultUser(_users);
    return save();
  }

  // Migrate legacy single-user format: {"hash":"..."}
  if (doc["hash"].is<const char*>() && !doc["users"].is<JsonArray>()) {
    Serial.println("[Auth] Migrating legacy auth config to multi-user format");
    _users.clear();
    _users[DEFAULT_USERNAME] = {doc["hash"].as<String>(), ""};  // empty salt = legacy
    return save();
  }

  if (!doc["users"].is<JsonArray>()) {
    Serial.println("[Auth] Unknown auth config format - resetting to default");
    makeDefaultUser(_users);
    return save();
  }

  _users.clear();
  for (JsonObject u : doc["users"].as<JsonArray>()) {
    const String uname = u["username"] | "";
    const String hash  = u["hash"]     | "";
    const String salt  = u["salt"]     | "";
    if (!uname.isEmpty() && !hash.isEmpty()) {
      _users[uname] = {hash, salt};
    }
  }

  if (_users.empty()) {
    Serial.println("[Auth] Auth config has no valid users - resetting to default");
    makeDefaultUser(_users);
    return save();
  }

  Serial.printf("[Auth] Loaded %zu admin account(s) from config\n", _users.size());
  return true;
}

bool AuthManager::checkCredentials(const String& username, const String& password) {
  const auto lockIt = _failedLogins.find(username);
  if (lockIt != _failedLogins.end() && lockIt->second.count >= MAX_FAILED_LOGINS) {
    if ((millis() - lockIt->second.firstFailAt) < LOCKOUT_DURATION_MS) {
      Serial.printf("[Auth] Login blocked for '%s' - too many failures\n", username.c_str());
      return false;
    }
    _failedLogins.erase(lockIt);
  }

  const auto it = _users.find(username);
  if (it == _users.end()) {
    trackFailure(username);  // also track unknown usernames to prevent timing-based enumeration
    return false;
  }

  const bool legacy = it->second.salt.isEmpty();
  const bool valid  = legacy
      ? (sha256hex(password)                       == it->second.hash)
      : (hashPassword(it->second.salt, password)   == it->second.hash);

  if (!valid) {
    trackFailure(username);
    return false;
  }

  _failedLogins.erase(username);

  // Upgrade legacy unsalted hash to salted on successful login
  if (legacy) {
    Serial.printf("[Auth] Upgrading legacy password hash for '%s' to salted SHA-256\n", username.c_str());
    const String newSalt = generateSalt();
    it->second.salt = newSalt;
    it->second.hash = hashPassword(newSalt, password);
    save();
  }

  return true;
}

String AuthManager::createSession(const String& username) {
  pruneExpiredSessions();
  if (_sessions.size() >= MAX_SESSIONS) {
    Serial.println("[Auth] Session table full - clearing all sessions");
    _sessions.clear();
  }

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
  return out;
}

bool AuthManager::addUser(const String& username, const String& password) {
  if (username.isEmpty()) return false;
  if (_users.count(username)) {
    Serial.printf("[Auth] addUser: '%s' already exists\n", username.c_str());
    return false;
  }
  const String salt = generateSalt();
  _users[username] = {hashPassword(salt, password), salt};
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
  const String salt = generateSalt();
  it->second = {hashPassword(salt, newPassword), salt};
  Serial.printf("[Auth] Password changed for '%s'\n", username.c_str());
  return save();
}

// Private

bool AuthManager::save() const {
  JsonDocument doc;
  JsonArray arr = doc["users"].to<JsonArray>();
  for (const auto& kv : _users) {
    JsonObject u = arr.add<JsonObject>();
    u["username"] = kv.first;
    u["hash"]     = kv.second.hash;
    u["salt"]     = kv.second.salt;
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

String AuthManager::generateSalt() {
  String salt;
  salt.reserve(32);
  for (int i = 0; i < 4; i++) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", (unsigned int)esp_random());
    salt += buf;
  }
  return salt;
}

String AuthManager::hashPassword(const String& salt, const String& password) {
  return sha256hex(salt + password);
}

void AuthManager::trackFailure(const String& username) {
  // Cap map size to prevent RAM exhaustion from login-spam with random usernames.
  if (_failedLogins.size() >= MAX_TRACKED_IPS && !_failedLogins.count(username)) {
    pruneExpiredLockouts();
    if (_failedLogins.size() >= MAX_TRACKED_IPS) {
      return;
    }
  }

  auto& fi = _failedLogins[username];
  if (fi.count == 0) fi.firstFailAt = millis();
  fi.count++;
  Serial.printf("[Auth] Failed login attempt for '%s' (%d/%d)\n",
                username.c_str(), fi.count, MAX_FAILED_LOGINS);
  if (fi.count >= MAX_FAILED_LOGINS) {
    Serial.printf("[Auth] Account '%s' locked for %lu s\n",
                  username.c_str(), LOCKOUT_DURATION_MS / 1000UL);
  }
}

void AuthManager::pruneExpiredLockouts() {
  const unsigned long now = millis();
  for (auto it = _failedLogins.begin(); it != _failedLogins.end(); ) {
    if ((now - it->second.firstFailAt) >= LOCKOUT_DURATION_MS) {
      it = _failedLogins.erase(it);
    } else {
      ++it;
    }
  }
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
