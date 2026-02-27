#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

#include "AuthManager.h"
#include "DeviceManager.h"

/**
 * Manages the async HTTP server.
 *
 * Public routes (no auth required):
 *   GET  /              - Main WOL launcher interface.
 *   GET  /login         - Login page.
 *   POST /api/login     - Validate password, set session cookie.
 *   POST /api/logout    - Destroy session cookie.
 *   GET  /api/devices   - JSON device list + online status (used by main page).
 *   POST /api/wake      - Send WOL magic packet.
 *
 * Protected routes (require valid wol_session cookie):
 *   GET    /admin         - Admin page (add / remove devices, change password).
 *   POST   /api/devices   - Add device; body: {"alias":"…","mac":"…","ip":"…"}.
 *   DELETE /api/devices   - Remove device; query param: ?mac=XX:XX:XX:XX:XX:XX.
 *   POST   /api/password  - Change password; body: {"current":"…","newPassword":"…"}.
 */
class WebServerManager {
 public:
  WebServerManager(DeviceManager& deviceManager, AuthManager& authManager);

  /** Register routes and start the server. Call once in setup(). */
  void begin();

  /** Invoked after a magic packet is sent; receives alias or raw MAC. */
  void setOnWakeCallback(std::function<void(const char* alias)> cb);

  /** Returns current online status for device[index]; used by GET /api/devices. */
  void setGetOnlineStatusCallback(std::function<bool(size_t index)> cb);

  /** Invoked after the device list changes; use to restart HostMonitor. */
  void setOnDeviceListChanged(std::function<void()> cb);

 private:
  AsyncWebServer _server;
  DeviceManager& _deviceManager;
  AuthManager&   _authManager;

  std::function<void(const char* alias)> _onWakeCallback;
  std::function<bool(size_t index)>      _getOnlineStatus;
  std::function<void()>                  _onDeviceListChanged;

  /** Returns true when the request carries a valid wol_session cookie. */
  bool isAuthenticated(AsyncWebServerRequest* request) const;

  void registerRoutes();
};
