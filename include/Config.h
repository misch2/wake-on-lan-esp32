#pragma once

// ── WiFi Manager captive portal ─────────────────────────────────────────────
// The device will broadcast this AP name when no credentials are saved.
// Connect to it, then navigate to 192.168.4.1 to enter your WiFi credentials.
// Credentials are stored in flash and reused on every subsequent boot.
#define WIFI_MANAGER_AP_NAME "WOL-Setup"
#define WIFI_MANAGER_AP_PASS ""  // leave empty for open portal

// ── Web Server ────────────────────────────────────────────────────────────────
#define WEB_SERVER_PORT 80

// ── Device list ───────────────────────────────────────────────────────────────
// Devices are now stored dynamically in LittleFS at /devices.json.
// Use the Admin page (http://<ip>/admin) to add or remove devices at runtime.
