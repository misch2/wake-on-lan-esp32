#include "HostMonitor.h"

#include <Arduino.h>

HostMonitor::HostMonitor() {}

HostMonitor::~HostMonitor() { stop(); }

// ── Public ────────────────────────────────────────────────────────────────────

void HostMonitor::begin(const std::vector<Device>& devices) {
  // Clean up any existing sessions first
  stop();

  const size_t n = devices.size();
  if (n == 0) return;

  // Pre-allocate all vectors before creating sessions so that no reallocation
  // happens while the ping callbacks hold pointers into _ctx.
  _online.assign(n, 0);
  _handles.assign(n, nullptr);
  _ctx.resize(n);
  _aliases.resize(n);
  _count = n;

  for (size_t i = 0; i < n; i++) {
    _ctx[i] = {this, i};
    _aliases[i] = devices[i].alias;

    const String& ipStr = devices[i].ip;
    if (ipStr.isEmpty()) {
      Serial.printf("[Ping] Device '%s' has no IP configured - skipping\n", devices[i].alias.c_str());
      continue;
    }

    ip_addr_t target;
    if (!ipaddr_aton(ipStr.c_str(), &target)) {
      Serial.printf("[Ping] Invalid IP '%s' for device '%s' - skipping\n", ipStr.c_str(), devices[i].alias.c_str());
      continue;
    }

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr = target;
    cfg.count = ESP_PING_COUNT_INFINITE;
    cfg.interval_ms = PING_INTERVAL_MS;
    cfg.timeout_ms = PING_TIMEOUT_MS;
    cfg.task_stack_size = PING_STACK_SIZE;

    esp_ping_callbacks_t cbs = {};
    cbs.on_ping_success = onSuccess;
    cbs.on_ping_timeout = onTimeout;
    cbs.cb_args = &_ctx[i];

    const esp_err_t err = esp_ping_new_session(&cfg, &cbs, &_handles[i]);
    if (err != ESP_OK) {
      Serial.printf("[Ping] Failed to create session for '%s': %s\n", devices[i].alias.c_str(), esp_err_to_name(err));
      _handles[i] = nullptr;
      continue;
    }

    esp_ping_start(_handles[i]);
    Serial.printf("[Ping] Monitoring '%s' at %s every %u ms\n", devices[i].alias.c_str(), ipStr.c_str(), PING_INTERVAL_MS);
  }
}

void HostMonitor::stop() {
  for (size_t i = 0; i < _count; i++) {
    if (_handles[i]) {
      esp_ping_stop(_handles[i]);
      esp_ping_delete_session(_handles[i]);
      _handles[i] = nullptr;
    }
  }
  _online.clear();
  _handles.clear();
  _ctx.clear();
  _aliases.clear();
  _count = 0;
}

void HostMonitor::restart(const std::vector<Device>& devices) { begin(devices); }

bool HostMonitor::isOnline(size_t index) const {
  if (index >= _count) return false;
  return _online[index] != 0;
}

void HostMonitor::setOnStatusChange(std::function<void()> cb) { _onStatusChange = cb; }

// ── Private ───────────────────────────────────────────────────────────────────

void HostMonitor::setOnline(size_t index, bool newState) {
  const uint8_t next = newState ? 1 : 0;
  if (_online[index] == next) return;  // no change, skip callback
  _online[index] = next;
  Serial.printf("[Ping] '%s' is now %s\n", _aliases[index].c_str(), newState ? "ONLINE" : "OFFLINE");
  if (_onStatusChange) _onStatusChange();
}

void HostMonitor::onSuccess(esp_ping_handle_t /*hdl*/, void* args) {
  auto* ctx = static_cast<PingContext*>(args);
  ctx->monitor->setOnline(ctx->index, true);
}

void HostMonitor::onTimeout(esp_ping_handle_t /*hdl*/, void* args) {
  auto* ctx = static_cast<PingContext*>(args);
  ctx->monitor->setOnline(ctx->index, false);
}
