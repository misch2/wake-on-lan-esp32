#pragma once

#include <lwip/ip_addr.h>
#include <ping/ping_sock.h>

#include <functional>
#include <vector>

#include "Device.h"

class HostMonitor {
 public:
  HostMonitor();
  ~HostMonitor();

  void begin(const std::vector<Device>& devices);
  void stop();
  void restart(const std::vector<Device>& devices);
  bool isOnline(size_t index) const;
  size_t deviceCount() const { return _count; }
  void setOnStatusChange(std::function<void()> cb);

 private:
  static constexpr uint32_t PING_INTERVAL_MS = 5000;
  static constexpr uint32_t PING_TIMEOUT_MS = 1500;
  static constexpr uint32_t PING_STACK_SIZE = 4096;

  struct PingContext {
    HostMonitor* monitor;
    size_t index;
  };

  size_t _count = 0;
  std::vector<uint8_t> _online;  // 0/1 per device (byte-sized for atomic write)
  std::vector<esp_ping_handle_t> _handles;
  std::vector<PingContext> _ctx;
  std::vector<String> _aliases;

  std::function<void()> _onStatusChange;

  static void onSuccess(esp_ping_handle_t hdl, void* args);
  static void onTimeout(esp_ping_handle_t hdl, void* args);

  void setOnline(size_t index, bool newState);
};
