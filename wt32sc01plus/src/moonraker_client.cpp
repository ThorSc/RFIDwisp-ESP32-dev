#include "moonraker_client.h"
#include "qidi_tag.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

static const uint32_t kTimeoutMs = 5000;

static MoonrakerResult httpGetJson(const String &url, JsonDocument &doc) {
  MoonrakerResult result;

  HTTPClient http;
  http.setTimeout(kTimeoutMs);
  if (!http.begin(url)) {
    result.error = "Could not open connection.";
    return result;
  }
  http.setConnectTimeout(kTimeoutMs);

  int status = http.GET();
  if (status <= 0) {
    result.error = "No response: " + http.errorToString(status);
    http.end();
    return result;
  }
  if (status >= 400) {
    result.error = "Moonraker returned HTTP " + String(status);
    http.end();
    return result;
  }

  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    result.error = "Malformed JSON response.";
    return result;
  }

  result.ok = true;
  return result;
}

static bool parseHexByte(char high, char low, uint8_t &out) {
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  int h = nibble(high), l = nibble(low);
  if (h < 0 || l < 0) return false;
  out = (uint8_t)((h << 4) | l);
  return true;
}

// Parses a 32-hex-char string into 16 bytes; false if malformed.
static bool parseTagHex(const String &hex, uint8_t out[qidiTagLength]) {
  if (hex.length() != qidiTagLength * 2) return false;
  for (size_t i = 0; i < qidiTagLength; i++) {
    if (!parseHexByte(hex[i * 2], hex[i * 2 + 1], out[i])) return false;
  }
  return true;
}

MoonrakerResult moonrakerGetBoxCount(const String &address, int &boxCount,
                                     bool &connected) {
  boxCount = 0;
  connected = false;

  JsonDocument doc;
  MoonrakerResult result =
      httpGetJson(address + "/printer/objects/query?multi_color_controller", doc);
  if (!result.ok) return result;

  JsonVariantConst hardware =
      doc["result"]["status"]["multi_color_controller"]["hardware"];
  if (hardware.isNull()) {
    result.ok = false;
    result.error = "Unexpected response shape (no multi_color_controller).";
    return result;
  }

  connected = hardware["connected"] | false;
  boxCount = connected ? (int)(hardware["box_count"] | 0) : 0;
  result.ok = true;
  return result;
}

MoonrakerResult moonrakerGetBoxSlots(const String &address, int boxNumber,
                                     QidiSlot out[4]) {
  for (int i = 0; i < 4; i++) out[i] = QidiSlot();

  JsonDocument doc;
  MoonrakerResult result =
      httpGetJson(address + "/printer/objects/query?multi_color_controller", doc);
  if (!result.ok) return result;

  JsonVariantConst slots = doc["result"]["status"]["multi_color_controller"]["slots"];
  JsonVariantConst states = slots["states"];
  JsonVariantConst materials = slots["materials"];
  if (states.isNull()) {
    result.ok = false;
    result.error = "Unexpected response shape (no slot states).";
    return result;
  }

  for (int i = 0; i < 4; i++) {
    int absoluteSlot = (boxNumber - 1) * 4 + i;
    String key = "slot" + String(absoluteSlot);
    int state = states[key] | 0;

    QidiSlot &slot = out[i];
    slot.slot = absoluteSlot;
    slot.loaded = state >= 1;
    slot.active = state == 2;

    if (slot.loaded) {
      JsonVariantConst material = materials[key];
      slot.material = (const char *)(material["filament"]["filament"] | "");
      slot.vendor = (const char *)(material["vendor"] | "");
      slot.colorHex = (const char *)(material["color"] | "");
    }
  }

  // The rfid_bridge extra is optional (a plain Klipper install won't have
  // it); its absence is not an error, it just leaves spoolNumber unset.
  JsonDocument rfidDoc;
  MoonrakerResult rfidResult =
      httpGetJson(address + "/printer/objects/query?rfid_bridge", rfidDoc);
  if (rfidResult.ok) {
    JsonVariantConst lastRaw = rfidDoc["result"]["status"]["rfid_bridge"]["last_raw"];
    if (!lastRaw.isNull()) {
      for (int i = 0; i < 4; i++) {
        int absoluteSlot = out[i].slot;
        JsonVariantConst hex = lastRaw["slot" + String(absoluteSlot)];
        if (hex.is<const char *>()) {
          uint8_t raw[qidiTagLength];
          if (parseTagHex(hex.as<const char *>(), raw)) {
            out[i].spoolNumber = decodeSpoolNumberFromRaw(raw);
            out[i].internalVendorId = decodeInternalVendorIdFromRaw(raw);
          }
        }
      }
    }
  }

  return result;
}
