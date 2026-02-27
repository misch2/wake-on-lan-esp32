#pragma once

#include <U8g2lib.h>
#include <Wire.h>

/**
 * Manages the SSD1306 72x40 OLED display (I2C, SCL=6, SDA=5).
 *
 * States and corresponding screens:
 *   Portal    – shown while WiFiManager captive portal is active
 *   Connected – shown during normal WOL server operation
 *   Waking    – briefly shown after a magic packet is dispatched, then
 *               automatically reverts to Connected
 *
 * Call update() from loop() to handle timed state transitions.
 */
class DisplayManager {
 public:
  DisplayManager();

  /** Initialise the display hardware. Call once in setup() before any show*(). */
  void begin();

  /** Show the WiFiManager captive portal screen. */
  void showPortal(const char* apName);

  /** Show the normal "ready" screen with SSID and IP address. */
  void showConnected(const char* ssid, const char* ip);

  /**
   * Briefly show a "Waking…" notification for the given device alias, then
   * automatically revert to the connected screen.
   * Safe to call from an async (non-loop) context – state is buffered and the
   * actual draw happens in update().
   */
  void showWaking(const char* alias);

  /** Must be called from loop() to handle timer-based screen transitions. */
  void update();

 private:
  U8G2_SSD1306_72X40_ER_F_HW_I2C _u8g2;

  enum class State { Portal, Connected, Waking };
  volatile State _state   = State::Portal;
  volatile bool  _redraw  = false;
  unsigned long  _wakingUntil = 0;

  static constexpr unsigned long WAKING_DURATION_MS = 3000;

  // Display hardware constants
  static constexpr uint8_t DISPLAY_WIDTH  = 72;
  static constexpr uint8_t DISPLAY_HEIGHT = 40;
  static constexpr uint8_t SCL_PIN        = 6;
  static constexpr uint8_t SDA_PIN        = 5;
  // Approximate max chars per line at font width 5 px
  static constexpr uint8_t CHARS_PER_LINE = DISPLAY_WIDTH / 5;

  // Buffered strings (written atomically enough for a display)
  char _ssid[33]   = {};
  char _ip[17]     = {};
  char _apName[33] = {};
  char _alias[33]  = {};

  void drawPortal();
  void drawConnected();
  void drawWaking();
  void drawCentered(uint8_t y, const char* str);
  void truncate(char* dst, const char* src, size_t maxLen);
};
