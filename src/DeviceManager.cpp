#include "DeviceManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

bool DeviceManager::begin() {
  if (!LittleFS.begin(true)) {  // format on first-boot
    Serial.println("[DevMgr] LittleFS mount failed - device list starts empty");
    return false;
  }

  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("[DevMgr] Config file not found - starting with empty device list");
    save();
    return true;
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[DevMgr] Cannot open config file for reading");
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[DevMgr] JSON parse error: %s - device list starts empty\n", err.c_str());
    return false;
  }

  _devices.clear();
  for (JsonObject obj : doc.as<JsonArray>()) {
    const String alias = obj["alias"] | "";
    const String mac   = obj["mac"]   | "";
    const String ip    = obj["ip"]    | "";
    if (alias.length() > 0 && mac.length() > 0) {
      _devices.push_back({alias, mac, ip});
    }
  }

  Serial.printf("[DevMgr] Loaded %zu device(s) from %s\n", _devices.size(), CONFIG_PATH);
  return true;
}

bool DeviceManager::save() const {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& d : _devices) {
    JsonObject obj = arr.add<JsonObject>();
    obj["alias"] = d.alias;
    obj["mac"]   = d.mac;
    obj["ip"]    = d.ip;
  }

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    Serial.println("[DevMgr] Cannot open config file for writing");
    return false;
  }

  serializeJson(doc, f);
  f.close();
  Serial.printf("[DevMgr] Saved %zu device(s) to %s\n", _devices.size(), CONFIG_PATH);
  return true;
}

bool DeviceManager::addDevice(const String& alias, const String& mac, const String& ip) {
  for (const auto& d : _devices) {
    if (d.mac.equalsIgnoreCase(mac)) {
      Serial.printf("[DevMgr] addDevice: MAC %s already exists\n", mac.c_str());
      return false;
    }
  }
  _devices.push_back({alias, mac, ip});
  return true;
}

bool DeviceManager::removeDevice(const String& mac) {
  for (auto it = _devices.begin(); it != _devices.end(); ++it) {
    if (it->mac.equalsIgnoreCase(mac)) {
      Serial.printf("[DevMgr] Removed device '%s'\n", it->alias.c_str());
      _devices.erase(it);
      return true;
    }
  }
  return false;
}
