#pragma once
#include <Arduino.h>
#include <vector>

// A saved Moonraker instance (see printer.dart). id is a stable slug
// ("printer1", "printer2", ...) assigned once at creation and never reused
// or changed, even if name/address are edited later.
struct Printer {
  String id;
  String name;
  String address; // Moonraker base URL, e.g. http://192.168.1.50:7125
};

// The subset of AppSettings from the Flutter app that this firmware needs.
// WiFi credentials are not stored here - WiFiManager keeps those in its own
// NVS namespace.
struct AppSettings {
  std::vector<Printer> printers;
  String selectedPrinterId; // may point to a deleted printer, see activePrinter()
  String spoolmanAddress = ""; // e.g. http://spoolman.local:7912
  bool useSpoolman = false;

  // The selected printer, or the first one if the selection is stale/unset,
  // or nullptr if there are no printers at all.
  Printer *activePrinter();

  // Adds a printer with a freshly-assigned id and selects it; returns it.
  Printer &addPrinter(const String &name, const String &address);

  // Removes the printer with this id, if any. If it was selected, the
  // selection falls back to the first remaining printer (or none).
  void removePrinter(const String &id);

  void load();
  void save() const;
};

extern AppSettings settings;
