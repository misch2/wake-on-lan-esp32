#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

/**
 * Manages the async HTTP server.
 * Routes:
 *   GET  /            - Serves the Bootstrap web interface.
 *   GET  /api/devices - Returns JSON array of configured devices.
 *   POST /api/wake    - Sends a WOL magic packet; requires ?mac=XX:XX:XX:XX:XX:XX.
 */
class WebServerManager {
 public:
  WebServerManager();

  /** Register routes and start the server. Call once in setup(). */
  void begin();

  /**
   * Optional callback invoked after a magic packet is successfully sent.
   * Receives the human-readable alias of the woken device (or the raw MAC if
   * no matching alias is found).
   */
  void setOnWakeCallback(std::function<void(const char* alias)> cb);

 private:
  AsyncWebServer _server;
  std::function<void(const char* alias)> _onWakeCallback;

  void registerRoutes();
};
