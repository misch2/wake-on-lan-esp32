#include <Arduino.h>
#include <WiFiManager.h>

#include "Config.h"
#include "DeviceManager.h"
#include "DisplayManager.h"
#include "HostMonitor.h"
#include "WebServerManager.h"

static DeviceManager deviceManager;
static DisplayManager displayManager;
static HostMonitor hostMonitor;
static WebServerManager webServerManager(deviceManager);

// ── WiFi ──────────────────────────────────────────────────────────────────────

static void connectWiFi() {
  WiFiManager wm;

  // Uncomment to erase stored credentials and force the portal on every boot:
  // wm.resetSettings();

  wm.setConfigPortalTimeout(180);  // close portal after 3 min if nobody connects

  Serial.printf("[WiFi] Starting WiFiManager portal '%s'\n", WIFI_MANAGER_AP_NAME);

  const bool connected = (strlen(WIFI_MANAGER_AP_PASS) > 0) ? wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASS) : wm.autoConnect(WIFI_MANAGER_AP_NAME);

  if (!connected) {
    Serial.println("[WiFi] Failed to connect – restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  Serial.printf("[WiFi] Connected!  IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Subnet:      %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[WiFi] Gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);  // allow USB serial to enumerate

  Serial.println("\n=== Wake-on-LAN Controller ===");

  // Load device list from LittleFS (mounts the filesystem on first boot)
  deviceManager.begin();

  displayManager.begin();

  // Show a boot/portal screen immediately (will be replaced once connected)
  displayManager.showPortal(WIFI_MANAGER_AP_NAME);
  displayManager.update();

  connectWiFi();

  // Small delay to let WiFiManager's synchronous WebServer fully release its
  // port-80 socket in the lwIP stack, avoiding the AsyncTCP "bind error: -8"
  // race condition on startup.
  delay(200);

  displayManager.showConnected(WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  displayManager.update();

  // ── HostMonitor ──────────────────────────────────────────────────────────
  hostMonitor.begin(deviceManager.devices());

  // ── Wire the WOL notification to the display ──────────────────────────────
  webServerManager.setOnWakeCallback([](const char* alias) {
    displayManager.showWaking(alias);
    displayManager.update();
  });

  // Provide online status to the /api/devices endpoint
  webServerManager.setGetOnlineStatusCallback([](size_t index) { return hostMonitor.isOnline(index); });

  // Restart HostMonitor whenever the device list changes via the admin page
  webServerManager.setOnDeviceListChanged([]() {
    Serial.println("[Main] Device list changed – restarting HostMonitor");
    hostMonitor.restart(deviceManager.devices());
  });

  webServerManager.begin();

  Serial.printf("[WOL] Open http://%s/ in your browser\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WOL] Admin: http://%s/admin\n", WiFi.localIP().toString().c_str());
}

void loop() {
  // Drive display state transitions (e.g. revert Waking → Connected after timeout)
  displayManager.update();
}
