#include "DisplayManager.h"

#include <cstring>


DisplayManager::DisplayManager()
    : _u8g2(U8G2_R0,
#ifdef PIN_OLED_RESET
            PIN_OLED_RESET,
#else
            U8X8_PIN_NONE,
#endif
            PIN_OLED_SCL, PIN_OLED_SDA) {
}


void DisplayManager::begin() {
  _u8g2.begin();
  _u8g2.setFont(u8g2_font_5x7_tf);
  _u8g2.setFontPosTop();
  Serial.printf("[OLED] Display initialised (%dx%d)\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void DisplayManager::showWiFiPortal(const char* apName) {
  truncate(_apName, apName, sizeof(_apName));
  _state = State::WiFiPortal;
  _redraw = true;
}

void DisplayManager::showWiFiReset() {
  _state = State::WiFiReset;
  _redraw = true;
}

void DisplayManager::showConnected(const char* ssid, const char* ip) {
  truncate(_ssid, ssid, sizeof(_ssid));
  truncate(_ip, ip, sizeof(_ip));
  _state = State::WiFiConnected;
  _redraw = true;
}

void DisplayManager::showWaking(const char* alias) {
  truncate(_alias, alias, sizeof(_alias));
  _wakingUntil = millis() + WAKING_DURATION_MS;
  _state = State::WOLInAction;
  _redraw = true;
}

void DisplayManager::update() {
  if (_state == State::WOLInAction && millis() >= _wakingUntil) {
    _state = State::WiFiConnected;
    _redraw = true;
  }

  if (!_redraw) return;
  _redraw = false;

  switch (_state) {
    case State::WiFiPortal:
      drawWiFiPortal();
      break;
    case State::WiFiReset:
      drawWiFiReset();
      break;
    case State::WiFiConnected:
      drawWiFiConnected();
      break;
    case State::WOLInAction:
      drawWaking();
      break;
  }
}


void DisplayManager::drawWiFiPortal() {
  _u8g2.clearBuffer();

  drawCentered(0, "WiFi Setup");
  _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
  drawCentered(13, "Connect to:");
  drawCentered(23, _apName);
  // drawCentered(33, "192.168.4.1");

  _u8g2.sendBuffer();
}

void DisplayManager::drawWiFiReset() {
  _u8g2.clearBuffer();

  drawCentered(0, "WiFi Cleared");
  _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
  drawCentered(13, "Resetting");
  drawCentered(23, "in");
  drawCentered(33, "5 seconds...");

  _u8g2.sendBuffer();
}

void DisplayManager::drawWiFiConnected() {
  _u8g2.clearBuffer();

  drawCentered(0, "Ready");
  _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
  drawCentered(22, "http://");
  drawCentered(30, _ip);

  _u8g2.sendBuffer();
}

void DisplayManager::drawWaking() {
  _u8g2.clearBuffer();

  drawCentered(0, ">> Waking <<");
  _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
  char truncAlias[CHARS_PER_LINE + 1];
  truncate(truncAlias, _alias, sizeof(truncAlias));
  drawCentered(20, truncAlias);
  drawCentered(32, "(WOL sent)");

  _u8g2.sendBuffer();
}

void DisplayManager::drawCentered(uint8_t y, const char* str) {
  uint8_t strW = _u8g2.getStrWidth(str);
  uint8_t x = (strW < DISPLAY_WIDTH) ? (DISPLAY_WIDTH - strW) / 2 : 0;
  _u8g2.drawStr(x, y, str);
}

void DisplayManager::truncate(char* dst, const char* src, size_t maxLen) {
  strncpy(dst, src, maxLen - 1);
  dst[maxLen - 1] = '\0';
}
