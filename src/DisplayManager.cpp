#include "DisplayManager.h"

#include <cstring>

// ── Constructor ───────────────────────────────────────────────────────────────
//  U8G2_R0          : 0° rotation
//  U8X8_PIN_NONE    : no hardware reset pin
//  6                : SCL
//  5                : SDA
DisplayManager::DisplayManager()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN) {}

// ── Public ────────────────────────────────────────────────────────────────────

void DisplayManager::begin() {
    _u8g2.begin();
    _u8g2.setFont(u8g2_font_5x7_tf);
    _u8g2.setFontPosTop(); // Y coordinates refer to the top of the glyph
    Serial.printf("[OLED] Display initialised (%dx%d)\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void DisplayManager::showPortal(const char* apName) {
    truncate(_apName, apName, sizeof(_apName));
    _state  = State::Portal;
    _redraw = true;
}

void DisplayManager::showConnected(const char* ssid, const char* ip) {
    truncate(_ssid, ssid, sizeof(_ssid));
    truncate(_ip,   ip,   sizeof(_ip));
    _state  = State::Connected;
    _redraw = true;
}

void DisplayManager::showWaking(const char* alias) {
    truncate(_alias, alias, sizeof(_alias));
    _wakingUntil = millis() + WAKING_DURATION_MS;
    _state  = State::Waking;
    _redraw = true;
}

void DisplayManager::update() {
    // Revert Waking → Connected after timeout
    if (_state == State::Waking && millis() >= _wakingUntil) {
        _state  = State::Connected;
        _redraw = true;
    }

    if (!_redraw) return;
    _redraw = false;

    switch (_state) {
        case State::Portal:    drawPortal();    break;
        case State::Connected: drawConnected(); break;
        case State::Waking:    drawWaking();    break;
    }
}

// ── Private drawing helpers ───────────────────────────────────────────────────

void DisplayManager::drawPortal() {
    _u8g2.clearBuffer();

    drawCentered(0, "WiFi Setup");
    _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
    drawCentered(13, "Connect to:");
    drawCentered(23, _apName);
    drawCentered(33, "192.168.4.1");

    _u8g2.sendBuffer();
}

void DisplayManager::drawConnected() {
    _u8g2.clearBuffer();

    drawCentered(0, "Ready");
    _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
    drawCentered(22, _ip);
    drawCentered(30, "http://^ /");

    _u8g2.sendBuffer();
}

void DisplayManager::drawWaking() {
    _u8g2.clearBuffer();

    drawCentered(0, ">> Waking <<");
    _u8g2.drawHLine(0, 10, DISPLAY_WIDTH);
    // Alias – truncate to fit DISPLAY_WIDTH px at ~5 px/char
    char truncAlias[CHARS_PER_LINE + 1];
    truncate(truncAlias, _alias, sizeof(truncAlias));
    drawCentered(20, truncAlias);
    drawCentered(32, "(WOL sent)");

    _u8g2.sendBuffer();
}

void DisplayManager::drawCentered(uint8_t y, const char* str) {
    uint8_t strW = _u8g2.getStrWidth(str);
    uint8_t x    = (strW < DISPLAY_WIDTH) ? (DISPLAY_WIDTH - strW) / 2 : 0;
    _u8g2.drawStr(x, y, str);
}

void DisplayManager::truncate(char* dst, const char* src, size_t maxLen) {
    strncpy(dst, src, maxLen - 1);
    dst[maxLen - 1] = '\0';
}
