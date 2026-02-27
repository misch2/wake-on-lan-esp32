#pragma once

#include "Device.h"

// ── WiFi Manager captive portal ─────────────────────────────────────────────
// The device will broadcast this AP name when no credentials are saved.
// Connect to it, then navigate to 192.168.4.1 to enter your WiFi credentials.
// Credentials are stored in flash and reused on every subsequent boot.
#define WIFI_MANAGER_AP_NAME "WOL-Setup"
#define WIFI_MANAGER_AP_PASS ""  // leave empty for open portal

// ── Web Server ────────────────────────────────────────────────────────────────
#define WEB_SERVER_PORT 80

// ── Device List ───────────────────────────────────────────────────────────────
// Add or remove devices here. MAC format: "AA:BB:CC:DD:EE:FF"
static const Device DEVICES[] = {
    {"Desktop PC", "AA:BB:CC:DD:EE:FF"},
    {"Gaming Rig", "11:22:33:44:55:66"},
    {"NAS Server", "DE:AD:BE:EF:00:01"},
};

static const size_t DEVICE_COUNT = sizeof(DEVICES) / sizeof(DEVICES[0]);
