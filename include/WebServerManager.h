#pragma once

#include <ESPAsyncWebServer.h>

#include <functional>

#include "DeviceManager.h"

/**
 * Manages the async HTTP server.
 *
 * Routes:
 *   GET  /              - Main Bootstrap web interface (WOL launcher).
 *   GET  /admin         - Admin page (add / remove devices).
 *   GET  /api/devices   - JSON array of current devices + online status.
 *   POST /api/devices   - Add a device; body: {"alias":"...","mac":"...","ip":"..."}.
 *   DELETE /api/devices - Remove a device; query param: ?mac=XX:XX:XX:XX:XX:XX.
 *   POST /api/wake      - Send WOL magic packet; query param: ?mac=XX:XX:XX:XX:XX:XX.
 */
class WebServerManager {
 public:
  explicit WebServerManager(DeviceManager& deviceManager);

  /** Register routes and start the server. Call once in setup(). */
  void begin();

  /**
   * Optional callback invoked after a magic packet is successfully sent.
   * Receives the human-readable alias of the woken device (or the raw MAC if
   * no matching alias is found).
   */
  void setOnWakeCallback(std::function<void(const char* alias)> cb);

  /**
   * Optional callback that returns the current online status for device[index].
   * When set, /api/devices includes an "online" boolean per entry.
   */
  void setGetOnlineStatusCallback(std::function<bool(size_t index)> cb);

  /**
   * Optional callback invoked after the device list changes (add or remove).
   * Use it to restart HostMonitor with the updated list.
   */
  void setOnDeviceListChanged(std::function<void()> cb);

 private:
  AsyncWebServer _server;
  DeviceManager& _deviceManager;

  std::function<void(const char* alias)> _onWakeCallback;
  std::function<bool(size_t index)> _getOnlineStatus;
  std::function<void()> _onDeviceListChanged;

  void registerRoutes();
};
