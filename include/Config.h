#pragma once

// ── WiFi Manager captive portal ─────────────────────────────────────────────
// The device will broadcast this AP name when no credentials are saved.
// Connect to it, then navigate to 192.168.4.1 to enter your WiFi credentials.
// Credentials are stored in flash and reused on every subsequent boot.
// AP name is derived at runtime: "WOL-" + last 3 MAC octets (e.g. "WOL-AABBCC")
#define WIFI_MANAGER_AP_PASS ""  // leave empty for open portal

// ── Web Server ────────────────────────────────────────────────────────────────
#define WEB_SERVER_PORT 80
