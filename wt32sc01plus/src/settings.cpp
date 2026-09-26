#include "settings.h"
#include <ArduinoJson.h>
#include <Preferences.h>

AppSettings settings;

static const char *kNamespace = "rfidwisp";

Printer *AppSettings::activePrinter() {
  if (printers.empty()) return nullptr;
  for (Printer &printer : printers) {
    if (printer.id == selectedPrinterId) return &printer;
  }
  return &printers[0];
}

Printer &AppSettings::addPrinter(const String &name, const String &address) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  uint32_t nextId = prefs.getUInt("nextPrinterId", 1);
  prefs.putUInt("nextPrinterId", nextId + 1);
  prefs.end();

  Printer printer;
  printer.id = "printer" + String(nextId);
  printer.name = name;
  printer.address = address;
  printers.push_back(printer);
  selectedPrinterId = printer.id;
  return printers.back();
}

void AppSettings::removePrinter(const String &id) {
  for (size_t i = 0; i < printers.size(); i++) {
    if (printers[i].id == id) {
      printers.erase(printers.begin() + i);
      break;
    }
  }
  if (selectedPrinterId == id) {
    selectedPrinterId = printers.empty() ? "" : printers[0].id;
  }
}

void AppSettings::load() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  String printersJson = prefs.getString("printers", "[]");
  selectedPrinterId = prefs.getString("selectedPrinterId", "");
  spoolmanAddress = prefs.getString("spoolmanAddr", "");
  useSpoolman = prefs.getBool("useSpoolman", false);
  prefs.end();

  printers.clear();
  JsonDocument doc;
  if (deserializeJson(doc, printersJson) == DeserializationError::Ok) {
    for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
      Printer printer;
      printer.id = (const char *)(item["id"] | "");
      printer.name = (const char *)(item["name"] | "");
      printer.address = (const char *)(item["address"] | "");
      if (printer.id.length()) printers.push_back(printer);
    }
  }
}

void AppSettings::save() const {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const Printer &printer : printers) {
    JsonObject item = array.add<JsonObject>();
    item["id"] = printer.id;
    item["name"] = printer.name;
    item["address"] = printer.address;
  }
  String printersJson;
  serializeJson(doc, printersJson);

  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("printers", printersJson);
  prefs.putString("selectedPrinterId", selectedPrinterId);
  prefs.putString("spoolmanAddr", spoolmanAddress);
  prefs.putBool("useSpoolman", useSpoolman);
  prefs.end();
}
