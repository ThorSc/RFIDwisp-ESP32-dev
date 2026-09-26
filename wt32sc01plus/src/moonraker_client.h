#pragma once
#include <Arduino.h>

// Minimal Moonraker REST client for the two endpoints the QIDI Data panel
// needs (see moonraker_api.dart): the box's hardware/slot status, and the
// optional Klipper rfid_bridge extra for the Spoolman spool number of each
// slot. HTTP GET only, blocking, ~5s timeout - matches the Flutter app.

struct QidiSlot {
  int slot = 0;      // 0-15, absolute slot number
  bool loaded = false;
  bool active = false;   // this slot is the one currently feeding
  String material;
  String vendor;
  String colorHex;       // "#RRGGBB", empty if unknown
  int spoolNumber = -1;      // Spoolman spool id, -1 = none/unknown
  int internalVendorId = -1; // Spoolman vendor id, -1 = none/unknown

  // Filled in by main.cpp from the Spoolman client (not by this module),
  // when Spoolman is enabled: the vendor name for internalVendorId, and the
  // spool's current remaining weight. -1 / empty if not looked up or unknown.
  String spoolmanVendorName;
  int weightGrams = -1;
};

struct MoonrakerResult {
  bool ok = false;
  String error; // set when ok == false
};

// Reads multi_color_controller.hardware.{box_count,connected}.
MoonrakerResult moonrakerGetBoxCount(const String &address, int &boxCount,
                                     bool &connected);

// Reads the 4 slots of one box (1-based boxNumber), filling out[0..3].
// Also merges in the Spoolman spool number/vendor id from the optional
// rfid_bridge object, if present - a missing rfid_bridge is not an error.
MoonrakerResult moonrakerGetBoxSlots(const String &address, int boxNumber,
                                     QidiSlot out[4]);
