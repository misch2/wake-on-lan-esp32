#include <Arduino.h>
#include <WiFiManager.h>

#include "AuthManager.h"
#include "Config.h"
#include "DeviceManager.h"
#include "DisplayManager.h"
#include "HostMonitor.h"
#include "WebServerManager.h"

static AuthManager authManager;
static DeviceManager deviceManager;
static DisplayManager displayManager;
static HostMonitor hostMonitor;
static WebServerManager webServerManager(deviceManager, authManager);

static char apName[16];  // "WOL-" + last 3 MAC octets, computed in setup()

// ── WiFi ──────────────────────────────────────────────────────────────────────

static void connectWiFi() {
  WiFiManager wm;

#ifdef PIN_WIFI_RESET
  if (digitalRead(PIN_WIFI_RESET) == LOW) {
    Serial.println("WiFi reset triggered - starting config portal...");
    wm.resetSettings();

    displayManager.showWiFiReset();
    displayManager.update();
    delay(5000);
    ESP.restart();
  }
#endif

  wm.setConfigPortalTimeout(180);  // close portal after 3 min if nobody connects

  Serial.printf("[WiFi] Starting WiFiManager portal '%s'\n", apName);

  const bool connected = (strlen(WIFI_MANAGER_AP_PASS) > 0) ? wm.autoConnect(apName, WIFI_MANAGER_AP_PASS) : wm.autoConnect(apName);

  if (!connected) {
    Serial.println("[WiFi] Failed to connect - restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  // Explicitly stop the WiFiManager web server to release the port-80 socket
  // before AsyncTCP tries to bind it.
  wm.stopWebPortal();
  delay(200);

  Serial.printf("[WiFi] Connected!  IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Subnet:      %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[WiFi] Gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);  // allow USB serial to enumerate

  Serial.println("\n=== Wake-on-LAN Controller ===");

  // Mount LittleFS and load persistent data (device list + auth config)
  deviceManager.begin();
  authManager.begin();

  displayManager.begin();

  // Derive AP name from last 3 MAC octets: "WOL-XXYYZZ"
  {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    snprintf(apName, sizeof(apName), "WOL-%s", mac.substring(6).c_str());
  }

  // Show a boot/portal screen immediately (will be replaced once connected)
  displayManager.showWiFiPortal(apName);
  displayManager.update();

#ifdef PIN_WIFI_RESET
  pinMode(PIN_WIFI_RESET, INPUT_PULLUP);
#endif

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
    Serial.println("[Main] Device list changed - restarting HostMonitor");
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
