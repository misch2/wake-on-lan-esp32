#pragma once

#include <lwip/ip_addr.h>
#include <ping/ping_sock.h>

#include <functional>
#include <vector>

#include "Device.h"

/**
 * Periodically pings all configured devices with known IPs using the ESP-IDF
 * async ICMP ping API.  No FreeRTOS task management is required from user
 * code - esp_ping runs its own internal task.
 *
 * The device list is supplied at runtime via begin(), allowing the list to be
 * refreshed (restart()) whenever it changes without rebooting the controller.
 *
 * Typical usage:
 *   hostMonitor.begin(deviceManager.devices());  // after WiFi is connected
 *
 * Then query isOnline(i) from any context.
 * After the device list changes, call restart(newDevices) to re-initialise.
 */
class HostMonitor {
 public:
  HostMonitor();
  ~HostMonitor();

  /**
   * Parse device IPs from `devices` and start a continuous ping session for
   * each device with a non-empty IP.  Call once after WiFi is associated.
   * Calling begin() while sessions are already running is safe - it calls
   * stop() first.
   */
  void begin(const std::vector<Device>& devices);

  /** Stop all active ping sessions and release resources. */
  void stop();

  /**
   * Convenience: stop then begin with a new device list.
   * Use this after the device list is mutated (add / remove).
   */
  void restart(const std::vector<Device>& devices);

  /** Returns true if the last ICMP probe to device[index] succeeded. */
  bool isOnline(size_t index) const;

  /** Returns the number of devices currently being monitored. */
  size_t deviceCount() const { return _count; }

  /**
   * Register a callback invoked (from the esp_ping task) whenever any
   * device's online status changes.  Keep it short - use it only to set
   * a flag or call display.showXxx() which just sets _redraw = true.
   */
  void setOnStatusChange(std::function<void()> cb);

 private:
  // ── Timing ──────────────────────────────────────────────────────────────
  static constexpr uint32_t PING_INTERVAL_MS = 5000;  ///< gap between probes
  static constexpr uint32_t PING_TIMEOUT_MS = 1500;   ///< time to wait for reply
  static constexpr uint32_t PING_STACK_SIZE = 4096;

  // ── Per-device state ─────────────────────────────────────────────────────
  struct PingContext {
    HostMonitor* monitor;
    size_t index;
  };

  size_t _count = 0;
  std::vector<uint8_t> _online;  ///< 0/1 per device (byte for atomic write)
  std::vector<esp_ping_handle_t> _handles;
  std::vector<PingContext> _ctx;
  std::vector<String> _aliases;  ///< local copy for logging

  std::function<void()> _onStatusChange;

  // ── esp_ping callbacks (called from internal task) ───────────────────────
  static void onSuccess(esp_ping_handle_t hdl, void* args);
  static void onTimeout(esp_ping_handle_t hdl, void* args);

  void setOnline(size_t index, bool newState);
};
