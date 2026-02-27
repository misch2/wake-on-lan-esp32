#pragma once

#include <vector>

#include "Device.h"

/**
 * Persists and manages the runtime device list using LittleFS.
 *
 * The device list is stored as a JSON array at CONFIG_PATH.
 * Call begin() once after filesystem is ready (inside setup()).
 * Call save() after any mutation to persist to flash.
 */
class DeviceManager {
 public:
  static constexpr const char* CONFIG_PATH = "/devices.json";

  /**
   * Mount LittleFS (formatting on first use) and load the device list.
   * If no config file exists the list starts empty and an empty file is saved.
   * Returns false on unrecoverable errors; the list is always left usable.
   */
  bool begin();

  /** Serialise the current device list to CONFIG_PATH. Returns false on I/O error. */
  bool save() const;

  /** Returns the current device list (read-only). */
  const std::vector<Device>& devices() const { return _devices; }

  /**
   * Append a new device.
   * Returns false (and does NOT modify the list) if a device with the same
   * MAC address already exists (case-insensitive comparison).
   */
  bool addDevice(const String& alias, const String& mac, const String& ip);

  /**
   * Remove the device whose MAC matches `mac` (case-insensitive).
   * Returns true if a device was removed, false if not found.
   */
  bool removeDevice(const String& mac);

 private:
  std::vector<Device> _devices;
};
