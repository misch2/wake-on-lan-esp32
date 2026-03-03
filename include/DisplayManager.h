#pragma once

#include <U8g2lib.h>
#include <Wire.h>

class DisplayManager {
 public:
  DisplayManager();

  void begin();
  void showWiFiPortal(const char* apName);
  void showWiFiReset();
  void showConnected(const char* ssid, const char* ip);
  void showWaking(const char* alias);
  void update();

 private:
  U8G2_SSD1306_72X40_ER_F_HW_I2C _u8g2;

  enum class State { WiFiPortal, WiFiReset, WiFiConnected, WOLInAction };
  volatile State _state = State::WiFiPortal;
  volatile bool _redraw = false;
  unsigned long _wakingUntil = 0;

  static constexpr unsigned long WAKING_DURATION_MS = 3000;
  static constexpr uint8_t DISPLAY_WIDTH = 72;
  static constexpr uint8_t DISPLAY_HEIGHT = 40;
  static constexpr uint8_t CHARS_PER_LINE = DISPLAY_WIDTH / 5;

  // Buffered strings for display
  char _ssid[33] = {};
  char _ip[17] = {};
  char _apName[33] = {};
  char _alias[33] = {};

  void drawWiFiPortal();
  void drawWiFiReset();
  void drawWiFiConnected();
  void drawWaking();
  void drawCentered(uint8_t y, const char* str);
  void truncate(char* dst, const char* src, size_t maxLen);
};
