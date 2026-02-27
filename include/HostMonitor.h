#pragma once

#include <functional>

#include <ping/ping_sock.h>
#include <lwip/ip_addr.h>

#include "Config.h"

/**
 * Periodically pings all configured devices with known IPs using the ESP-IDF
 * async ICMP ping API.  No FreeRTOS task management is required from user
 * code – esp_ping runs its own internal task.
 *
 * Typical usage:
 *   hostMonitor.setOnStatusChange([] { /* redraw display  *\/ });
 *   hostMonitor.begin();  // after WiFi is connected
 *
 * Then query isOnline(i) from any context.
 */
class HostMonitor {
 public:
  HostMonitor();
  ~HostMonitor();

  /**
   * Parse device IPs and start a continuous ping session for each device
   * with a non-empty IP.  Call once after WiFi is associated.
   */
  void begin();

  /** Returns true if the last ICMP probe to device[index] succeeded. */
  bool isOnline(size_t index) const;

  /**
   * Register a callback invoked (from the esp_ping task) whenever any
   * device's online status changes.  Keep it short – use it only to set
   * a flag or call display.showXxx() which just sets _redraw = true.
   */
  void setOnStatusChange(std::function<void()> cb);

 private:
  // ── Timing ──────────────────────────────────────────────────────────────
  static constexpr uint32_t PING_INTERVAL_MS = 5000;  ///< gap between probes
  static constexpr uint32_t PING_TIMEOUT_MS  = 1500;  ///< time to wait for reply
  static constexpr uint32_t PING_STACK_SIZE  = 4096;

  // ── Per-device state ─────────────────────────────────────────────────────
  struct PingContext {
    HostMonitor* monitor;
    size_t       index;
  };

  volatile bool         _online[DEVICE_COUNT] = {};
  esp_ping_handle_t     _handles[DEVICE_COUNT] = {};
  PingContext           _ctx[DEVICE_COUNT];

  std::function<void()> _onStatusChange;

  // ── esp_ping callbacks (called from internal task) ───────────────────────
  static void onSuccess(esp_ping_handle_t hdl, void* args);
  static void onTimeout(esp_ping_handle_t hdl, void* args);

  void setOnline(size_t index, bool newState);
};
