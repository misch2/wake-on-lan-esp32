#pragma once

#include <ESPAsyncWebServer.h>

#include <functional>

#include "AuthManager.h"
#include "DeviceManager.h"

class WebServerManager {
 public:
  WebServerManager(DeviceManager& deviceManager, AuthManager& authManager);

  void begin();
  void setOnWakeCallback(std::function<void(const char* alias)> cb);
  void setGetOnlineStatusCallback(std::function<bool(size_t index)> cb);
  void setOnDeviceListChanged(std::function<void()> cb);

 private:
  AsyncWebServer _server;
  DeviceManager& _deviceManager;
  AuthManager& _authManager;

  std::function<void(const char* alias)> _onWakeCallback;
  std::function<bool(size_t index)> _getOnlineStatus;
  std::function<void()> _onDeviceListChanged;

  bool isAuthenticated(AsyncWebServerRequest* request) const;
  String extractToken(AsyncWebServerRequest* request) const;
  void registerRoutes();
};
