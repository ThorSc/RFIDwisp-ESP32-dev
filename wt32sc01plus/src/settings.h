#pragma once
#include <Arduino.h>

// The subset of AppSettings from the Flutter app that this firmware needs.
// WiFi credentials are not stored here - WiFiManager keeps those in its own
// NVS namespace.
struct AppSettings {
  String spoolmanAddress = ""; // e.g. http://spoolman.local:7912
  bool useSpoolman = false;
  uint8_t sleepMinutes = 10; // screen sleep timeout, 0-60 minutes, 0 = never

  void load();
  void save() const;
};

extern AppSettings settings;
