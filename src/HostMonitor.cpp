#include "HostMonitor.h"

#include <Arduino.h>
#include <cstring>

HostMonitor::HostMonitor() {
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        _online[i]  = false;
        _handles[i] = nullptr;
        _ctx[i]     = { this, i };
    }
}

HostMonitor::~HostMonitor() {
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        if (_handles[i]) {
            esp_ping_stop(_handles[i]);
            esp_ping_delete_session(_handles[i]);
        }
    }
}

// ── Public ────────────────────────────────────────────────────────────────────

void HostMonitor::begin() {
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const char* ipStr = DEVICES[i].ip;
        if (!ipStr || ipStr[0] == '\0') {
            Serial.printf("[Ping] Device '%s' has no IP configured – skipping\n",
                          DEVICES[i].alias);
            continue;
        }

        ip_addr_t target;
        if (!ipaddr_aton(ipStr, &target)) {
            Serial.printf("[Ping] Invalid IP '%s' for device '%s' – skipping\n",
                          ipStr, DEVICES[i].alias);
            continue;
        }

        esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
        cfg.target_addr   = target;
        cfg.count         = ESP_PING_COUNT_INFINITE;
        cfg.interval_ms   = PING_INTERVAL_MS;
        cfg.timeout_ms    = PING_TIMEOUT_MS;
        cfg.task_stack_size = PING_STACK_SIZE;

        esp_ping_callbacks_t cbs = {};
        cbs.on_ping_success = onSuccess;
        cbs.on_ping_timeout = onTimeout;
        cbs.cb_args         = &_ctx[i];

        esp_err_t err = esp_ping_new_session(&cfg, &cbs, &_handles[i]);
        if (err != ESP_OK) {
            Serial.printf("[Ping] Failed to create session for '%s': %s\n",
                          DEVICES[i].alias, esp_err_to_name(err));
            _handles[i] = nullptr;
            continue;
        }

        esp_ping_start(_handles[i]);
        Serial.printf("[Ping] Monitoring '%s' at %s every %u ms\n",
                      DEVICES[i].alias, ipStr, PING_INTERVAL_MS);
    }
}

bool HostMonitor::isOnline(size_t index) const {
    if (index >= DEVICE_COUNT) return false;
    return _online[index];
}

void HostMonitor::setOnStatusChange(std::function<void()> cb) {
    _onStatusChange = cb;
}

// ── Private ───────────────────────────────────────────────────────────────────

void HostMonitor::setOnline(size_t index, bool newState) {
    if (_online[index] == newState) return;  // no change, skip callback
    _online[index] = newState;
    Serial.printf("[Ping] '%s' is now %s\n",
                  DEVICES[index].alias, newState ? "ONLINE" : "OFFLINE");
    if (_onStatusChange) _onStatusChange();
}

// Called from esp_ping internal task on successful ICMP reply
void HostMonitor::onSuccess(esp_ping_handle_t /*hdl*/, void* args) {
    auto* ctx = static_cast<PingContext*>(args);
    ctx->monitor->setOnline(ctx->index, true);
}

// Called from esp_ping internal task on timeout (no reply)
void HostMonitor::onTimeout(esp_ping_handle_t /*hdl*/, void* args) {
    auto* ctx = static_cast<PingContext*>(args);
    ctx->monitor->setOnline(ctx->index, false);
}
