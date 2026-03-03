#pragma once

#include <vector>

#include "Device.h"

class DeviceManager {
 public:
  static constexpr const char* CONFIG_PATH = "/devices.json";

  // Mounts LittleFS (formatting on first use) and loads the device list.
  bool begin();
  bool save() const;
  const std::vector<Device>& devices() const { return _devices; }
  bool addDevice(const String& alias, const String& mac, const String& ip);
  bool removeDevice(const String& mac);

 private:
  std::vector<Device> _devices;
};
