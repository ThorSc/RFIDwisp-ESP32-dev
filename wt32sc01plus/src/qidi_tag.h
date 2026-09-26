#pragma once
#include <Arduino.h>

// The 16-byte spool payload a QIDI box reads from sector 1, block 4 of a
// MIFARE Classic 1K tag, and the material/colour/vendor tables it is built
// from. Ported from RFIDwisp's filament_data.dart and filament_spool.dart -
// keep both in sync if the tag format ever changes.

struct NamedByte {
  const char *name;
  uint8_t code;
};

extern const NamedByte qidiMaterials[];
extern const size_t qidiMaterialsCount;

extern const NamedByte qidiColors[]; // name is "#RRGGBB"
extern const size_t qidiColorsCount;

extern const NamedByte qidiVendors[];
extern const size_t qidiVendorsCount;

const char *qidiMaterialName(uint8_t code);
const char *qidiColorHex(uint8_t code);
const char *qidiVendorName(uint8_t code);
int qidiMaterialCode(const char *name);   // -1 if unknown
int qidiColorCode(const char *hex);       // -1 if unknown
int qidiVendorCode(const char *name);     // -1 if unknown

// The sector QIDI boxes keep spool data in; the payload lives in its first
// block (sector * 4).
constexpr uint8_t qidiTagSector = 1;
constexpr uint8_t qidiTagBlock = qidiTagSector * 4;
constexpr size_t qidiTagLength = 16;

struct FilamentSpool {
  uint8_t materialCode = 0;
  uint8_t colorCode = 0;
  uint8_t vendorCode = 0;   // QIDI vendor code; ignored if internalVendorId != 0
  uint16_t spoolNumber = 0; // 0-999
  uint8_t internalVendorId = 0; // Spoolman vendor id, 0 = none
  uint16_t lastWeightGrams = 0;

  // Packs this spool into the 16-byte tag payload (see reader_service.dart /
  // filament_spool.dart for the authoritative layout):
  //   byte 0: material, byte 1: colour, byte 2: vendor (or Spoolman vendor id)
  //   bytes 3-10: zero, bytes 11-12: last weight (big-endian)
  //   byte 13: Spoolman vendor id (0 = none), bytes 14-15: spool number (big-endian)
  void toTagBytes(uint8_t out[qidiTagLength]) const {
    memset(out, 0, qidiTagLength);
    out[0] = materialCode;
    out[1] = colorCode;
    out[2] = internalVendorId != 0 ? internalVendorId : vendorCode;
    out[11] = (lastWeightGrams >> 8) & 0xFF;
    out[12] = lastWeightGrams & 0xFF;
    out[13] = internalVendorId;
    out[14] = (spoolNumber >> 8) & 0xFF;
    out[15] = spoolNumber & 0xFF;
  }

  // Decodes a 16-byte tag payload. Returns false if it does not describe a
  // spool this firmware (or RFIDwisp) could have written.
  static bool fromTagBytes(const uint8_t in[qidiTagLength], FilamentSpool &out) {
    uint8_t internalVendorId = in[13];
    uint8_t vendorCode = internalVendorId != 0 ? qidiVendorCode("GENERIC") : in[2];

    if (qidiMaterialName(in[0]) == nullptr) return false;
    if (qidiColorHex(in[1]) == nullptr) return false;
    if (qidiVendorName(vendorCode) == nullptr) return false;

    out.materialCode = in[0];
    out.colorCode = in[1];
    out.vendorCode = vendorCode;
    out.internalVendorId = internalVendorId;
    out.spoolNumber = (uint16_t(in[14]) << 8) | in[15];
    out.lastWeightGrams = (uint16_t(in[11]) << 8) | in[12];
    return true;
  }

  static bool isBlankTagBytes(const uint8_t in[qidiTagLength]) {
    for (size_t i = 0; i < qidiTagLength; i++) {
      if (in[i] != 0) return false;
    }
    return true;
  }
};

// Raw byte 13 / bytes 14-15 of a tag payload, without requiring the rest to
// decode to a known material/colour/vendor - this is what the Klipper
// rfid_bridge module and moonraker_api.dart use to report a slot's Spoolman
// spool number even for a box that doesn't otherwise show full spool data.
inline int decodeSpoolNumberFromRaw(const uint8_t raw[qidiTagLength]) {
  uint16_t value = (uint16_t(raw[14]) << 8) | raw[15];
  return value == 0 ? -1 : (int)value;
}

inline int decodeInternalVendorIdFromRaw(const uint8_t raw[qidiTagLength]) {
  return raw[13] == 0 ? -1 : (int)raw[13];
}

// Ports of filament_data.dart's closestQidiColor / qidiMaterialChoices: used
// to prefill the QIDI-side fields from a free-form Spoolman spool (whose
// material/colour are arbitrary strings, not one of the fixed QIDI codes).
uint8_t closestQidiColorCode(const char *hex); // hex: "#RRGGBB" or "RRGGBB"
uint8_t closestQidiMaterialCode(const char *material); // exact or substring match, else qidiMaterials[0]
