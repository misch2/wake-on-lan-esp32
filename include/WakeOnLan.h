#pragma once

#include <Arduino.h>

/**
 * Sends Wake-on-LAN magic packets over UDP broadcast.
 *
 * A magic packet consists of:
 *   - 6 bytes of 0xFF
 *   - 16 repetitions of the target 6-byte MAC address
 * Total: 102 bytes, sent to UDP broadcast on port 9.
 */
class WakeOnLan {
 public:
  /**
   * Send a WOL magic packet to the given MAC address.
   * @param macAddress MAC address string in "AA:BB:CC:DD:EE:FF" format.
   * @return true if the packet was sent; false if the MAC string is invalid.
   */
  static bool wake(const char* macAddress);

 private:
  static bool parseMac(const char* macStr, uint8_t* outMac);
  static void buildMagicPacket(const uint8_t* mac, uint8_t* outPacket);
};
