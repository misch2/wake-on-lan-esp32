#include "WakeOnLan.h"

#include <WiFiUdp.h>

namespace {
constexpr uint16_t WOL_PORT = 9;
constexpr size_t MAC_BYTES = 6;
constexpr size_t MAC_REPETITIONS = 16;
constexpr size_t MAGIC_PKT_LEN = MAC_BYTES + MAC_BYTES * MAC_REPETITIONS;  // 102
}  // namespace

bool WakeOnLan::wake(const char* macAddress) {
  uint8_t mac[MAC_BYTES];
  if (!parseMac(macAddress, mac)) {
    Serial.printf("[WOL] Invalid MAC address: %s\n", macAddress);
    return false;
  }

  uint8_t packet[MAGIC_PKT_LEN];
  buildMagicPacket(mac, packet);

  WiFiUDP udp;
  udp.begin(WOL_PORT);
  udp.beginPacket(IPAddress(255, 255, 255, 255), WOL_PORT);
  udp.write(packet, MAGIC_PKT_LEN);
  udp.endPacket();
  udp.stop();

  Serial.printf("[WOL] Magic packet sent to %s\n", macAddress);
  return true;
}

bool WakeOnLan::parseMac(const char* macStr, uint8_t* outMac) {
  // Expect exactly "XX:XX:XX:XX:XX:XX" (17 chars)
  if (strlen(macStr) != 17) return false;

  for (size_t i = 0; i < MAC_BYTES; i++) {
    const char* pos = macStr + (i * 3);
    if (!isxdigit((unsigned char)pos[0]) || !isxdigit((unsigned char)pos[1])) return false;
    char hexByte[3] = {pos[0], pos[1], '\0'};
    outMac[i] = static_cast<uint8_t>(strtol(hexByte, nullptr, 16));

    // Validate separator between bytes (skip check after last byte)
    if (i < MAC_BYTES - 1 && pos[2] != ':') return false;
  }
  return true;
}

void WakeOnLan::buildMagicPacket(const uint8_t* mac, uint8_t* outPacket) {
  memset(outPacket, 0xFF, MAC_BYTES);
  for (size_t i = 0; i < MAC_REPETITIONS; i++) {
    memcpy(outPacket + MAC_BYTES + i * MAC_BYTES, mac, MAC_BYTES);
  }
}
