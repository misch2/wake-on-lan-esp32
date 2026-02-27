#pragma once

/**
 * Represents a network device that can be woken via Wake-on-LAN.
 */
struct Device {
  const char* alias;  ///< Human-readable name shown in the UI
  const char* mac;    ///< MAC address in "AA:BB:CC:DD:EE:FF" format
};
