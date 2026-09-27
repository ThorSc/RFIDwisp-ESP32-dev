#pragma once
#include <Arduino.h>

// The subset of AppSettings from the Flutter app that this firmware needs.
// WiFi credentials are not stored here - WiFiManager keeps those in its own
// NVS namespace.
struct AppSettings {
  String spoolmanAddress = ""; // e.g. http://spoolman.local:7912
  bool useSpoolman = false;

  void load();
  void save() const;
};

extern AppSettings settings;
