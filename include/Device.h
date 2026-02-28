#pragma once

#include <WString.h>

struct Device {
  String alias;  ///< Human-readable name shown in the UI
  String mac;    ///< MAC address in "AA:BB:CC:DD:EE:FF" format
  String ip;     ///< IPv4 address in dotted-decimal, or "" if unknown
};
