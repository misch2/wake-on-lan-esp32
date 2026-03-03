#pragma once

#include <Arduino.h>

class WakeOnLan {
 public:
  static bool wake(const char* macAddress);

 private:
  static bool parseMac(const char* macStr, uint8_t* outMac);
  static void buildMagicPacket(const uint8_t* mac, uint8_t* outPacket);
};
