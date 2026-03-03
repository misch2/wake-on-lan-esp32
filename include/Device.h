#pragma once

#include <WString.h>

struct Device {
  String alias;
  String mac;  // "AA:BB:CC:DD:EE:FF" format
  String ip;   // dotted-decimal or empty
};
