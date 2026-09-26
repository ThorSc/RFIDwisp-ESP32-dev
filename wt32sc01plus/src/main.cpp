#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "display_setup.h"
#include "moonraker_client.h"
#include "pn532_reader.h"
#include "qidi_tag.h"
#include "settings.h"
#include "spoolman_client.h"
#include "strings.h"
#include "ui.h"
#include "wifi_setup.h"

static Pn532Reader reader;
static int currentBox = 1;

// Cached Spoolman data, refreshed whenever the write screen (or a tag read)
// is about to show it. filaments is only used internally here (matching a
// new spool to link); vendors/spools are also handed to the UI for its
// dropdowns.
static std::vector<SpoolmanVendor> spoolmanVendors;
static std::vector<SpoolmanFilament> spoolmanFilaments;
static std::vector<SpoolmanSpool> spoolmanSpools;

// The spool number of the last tag read, if any - used as a fallback when
// writing a "new" spool that no longer matches any cached Spoolman filament
// (mirrors rfid_tag_panel.dart's "Spoolman skipped" path for a re-write).
static int lastReadSpoolNumber = -1;

static bool spoolmanActive() {
  return settings.useSpoolman && settings.spoolmanAddress.length() > 0;
}

// Reloads the Spoolman caches and pushes them to the UI; returns whether
// Spoolman mode should be active (false also on a load failure, so the form
// falls back to plain manual entry rather than showing stale/empty lists).
static bool refreshSpoolmanData() {
  if (!spoolmanActive()) return false;

  uiSetStatus(T(StrId::LoadingSpoolmanData));
  displayLoop();

  bool ok = spoolmanGetVendors(settings.spoolmanAddress, spoolmanVendors).ok &&
            spoolmanGetFilaments(settings.spoolmanAddress, spoolmanFilaments).ok &&
            spoolmanGetSpools(settings.spoolmanAddress, spoolmanSpools).ok;
  if (!ok) {
    uiSetStatus(T(StrId::SpoolmanLoadFailed));
    return false;
  }

  uiSetSpoolmanLists(spoolmanVendors, spoolmanSpools, spoolmanFilaments);
  return true;
}

static const char *tagResultMessage(TagResult result) {
  switch (result) {
    case TagResult::Ok: return "OK";
    case TagResult::NoTag: return T(StrId::ErrorNoTag);
    case TagResult::NotMifareClassic1k: return T(StrId::ErrorNotClassic1k);
    case TagResult::AuthenticationFailed: return T(StrId::ErrorAuthFailed);
    case TagResult::ReadFailed: return T(StrId::ErrorReadFailed);
    case TagResult::WriteFailed: return T(StrId::ErrorWriteFailed);
  }
  return "";
}

// Called by the UI's "Read tag" button. Blocking: the display will not
// update again until this returns, which is fine for a single explicit
// user action but would need a background task for anything longer-running.
void handleReadTagRequested() {
  uiSetStatus(T(StrId::StatusHoldTag));
  displayLoop();

  if (!reader.waitForTag(3000)) {
    uiSetStatus(tagResultMessage(TagResult::NoTag));
    return;
  }

  uint8_t raw[qidiTagLength];
  TagResult result = reader.readSpoolBytes(raw);
  if (result != TagResult::Ok) {
    uiSetStatus(tagResultMessage(result));
    return;
  }

  FilamentSpool spool;
  if (FilamentSpool::isBlankTagBytes(raw)) {
    uiSetStatus(T(StrId::TagBlank));
    return;
  }
  if (!FilamentSpool::fromTagBytes(raw, spool)) {
    uiSetStatus(T(StrId::ErrorUnknownTag));
    return;
  }

  lastReadSpoolNumber = spool.spoolNumber > 0 ? spool.spoolNumber : -1;
  bool active = refreshSpoolmanData();
  uiSetSpoolmanMode(active);

  uiSetStatus(T(StrId::TagRead));
  uiShowSpool(spool);
}

// Called by the main screen's "Write tag" button, before the edit screen is
// shown - refreshes the Spoolman lists (if active) so the Spool selector is
// current, then hands off to the UI.
void handleWriteScreenOpened() {
  lastReadSpoolNumber = -1;
  bool active = refreshSpoolmanData();
  uiSetSpoolmanMode(active);
  uiSetStatus(T(StrId::Ready));
  uiShowEditScreenForWrite();
}

// Resolves the Spoolman side of a write: for an existing spool, just reuses
// its id/vendor; for a new one, tries to link it to a matching filament and
// create it in Spoolman, falling back to a plain (unlinked) tag write if
// that is not possible. No-op if Spoolman is not active.
static void resolveSpoolmanFields(FilamentSpool &spool) {
  if (!spoolmanActive()) return;

  if (!uiIsNewSpoolSelected()) {
    int id = uiSelectedExistingSpoolId();
    int vendorId = uiSelectedSpoolmanVendorId();
    spool.spoolNumber = id > 0 ? (uint16_t)id : 0;
    spool.internalVendorId = vendorId > 0 ? (uint8_t)vendorId : 0;
    return;
  }

  int vendorId = uiSelectedSpoolmanVendorId();
  int filamentId = vendorId >= 0
      ? findFilamentId(spoolmanFilaments, vendorId, qidiMaterialName(spool.materialCode),
                        qidiColorHex(spool.colorCode))
      : -1;

  if (filamentId < 0) {
    uiSetStatus(T(StrId::SpoolmanSkipped));
    spool.spoolNumber = lastReadSpoolNumber > 0 ? (uint16_t)lastReadSpoolNumber : 0;
    spool.internalVendorId = 0;
    return;
  }

  int newId;
  SpoolmanResult result =
      spoolmanCreateSpool(settings.spoolmanAddress, filamentId, spool.lastWeightGrams, newId);
  if (!result.ok) {
    uiSetStatus((String(T(StrId::SpoolmanCreateFailed)) + ": " + result.error).c_str());
    spool.spoolNumber = lastReadSpoolNumber > 0 ? (uint16_t)lastReadSpoolNumber : 0;
    spool.internalVendorId = 0;
    return;
  }

  spool.spoolNumber = (uint16_t)newId;
  spool.internalVendorId = (uint8_t)vendorId;
  uiSetStatus(T(StrId::SpoolCreated));
}

// Called by the edit screen's "Write tag" button.
void handleWriteTagRequested() {
  FilamentSpool spool = uiCurrentSpool();
  resolveSpoolmanFields(spool);

  uiSetStatus(T(StrId::StatusHoldTag));
  displayLoop();

  if (!reader.waitForTag(3000)) {
    uiSetStatus(tagResultMessage(TagResult::NoTag));
    return;
  }

  uint8_t raw[qidiTagLength];
  spool.toTagBytes(raw);
  TagResult result = reader.writeSpoolBytes(raw);
  uiSetStatus(result == TagResult::Ok ? T(StrId::TagWritten) : tagResultMessage(result));
}

static void readQidiBox(int boxNumber) {
  Printer *printer = settings.activePrinter();
  if (!printer) return;

  QidiSlot slots[4];
  MoonrakerResult result = moonrakerGetBoxSlots(printer->address, boxNumber, slots);
  if (!result.ok) {
    uiSetQidiPrinterStatus((String(T(StrId::PrinterNotConnected)) + ": " + result.error).c_str());
    return;
  }
  uiSetQidiPrinterStatus(T(StrId::PrinterConnected));

  // Spoolman vendor-name/weight lookups (rfid_bridge only reports the
  // Spoolman spool/vendor ids, not their names or current weight).
  if (spoolmanActive()) {
    std::vector<SpoolmanVendor> vendors;
    spoolmanGetVendors(settings.spoolmanAddress, vendors); // best-effort; ignore failure
    for (int i = 0; i < 4; i++) {
      QidiSlot &slot = slots[i];
      if (slot.internalVendorId >= 0) {
        for (const SpoolmanVendor &vendor : vendors) {
          if (vendor.id == slot.internalVendorId) {
            slot.spoolmanVendorName = vendor.name;
            break;
          }
        }
      }
      if (slot.spoolNumber >= 0) {
        int weight = spoolmanGetRemainingWeight(settings.spoolmanAddress, slot.spoolNumber);
        if (weight >= 0) slot.weightGrams = weight;
      }
    }
  }

  uiSetQidiSlots(slots);
}

// Called by the QIDI Data screen when it is opened, and whenever the printer
// selector on it changes.
void handleQidiScreenOpened() {
  uiSetQidiPrinterList(settings.printers, settings.selectedPrinterId);

  Printer *printer = settings.activePrinter();
  if (!printer || printer->address.length() == 0) {
    uiSetQidiPrinterStatus(T(StrId::NoMoonrakerConfigured));
    uiSetQidiBoxCount(0);
    return;
  }

  uiSetQidiPrinterStatus(T(StrId::PrinterChecking));
  displayLoop();

  int boxCount;
  bool connected;
  MoonrakerResult result = moonrakerGetBoxCount(printer->address, boxCount, connected);
  if (!result.ok) {
    uiSetQidiPrinterStatus((String(T(StrId::PrinterNotConnected)) + ": " + result.error).c_str());
    uiSetQidiBoxCount(0);
    return;
  }
  if (!connected) {
    uiSetQidiPrinterStatus(T(StrId::PrinterNotConnected));
    uiSetQidiBoxCount(0);
    return;
  }

  uiSetQidiPrinterStatus(T(StrId::PrinterConnected));
  uiSetQidiBoxCount(boxCount);
  if (boxCount > 0) {
    currentBox = 1;
    readQidiBox(currentBox);
  }
}

// Called by the QIDI Data screen's printer dropdown.
void handleQidiPrinterSelected(const String &printerId) {
  settings.selectedPrinterId = printerId;
  settings.save();
  handleQidiScreenOpened();
}

// Called by the QIDI Data screen's box dropdown.
void handleQidiBoxSelected(int boxNumber) {
  currentBox = boxNumber;
  readQidiBox(currentBox);
}

// Called by the QIDI Data screen's "Read box" button.
void handleQidiReadBoxRequested() { readQidiBox(currentBox); }

static void connectWifi() {
  uiSetStatus("Connecting to Wi-Fi...");
  displayLoop();

  bool connected = wifiSetupConnect();
  if (connected) {
    String info = String(T(StrId::WifiStatus)) + ": " + WiFi.SSID() + " (" +
                  WiFi.localIP().toString() + ")";
    uiSetWifiStatus(info.c_str());
  } else {
    uiSetWifiStatus("Not connected - join \"RFIDwisp-Setup\" to configure.");
  }
}

void setup() {
  Serial.begin(115200);

  stringsInit();
  settings.load();

  displaySetup();
  uiInit();

  connectWifi();

  if (!reader.begin()) {
    uiSetStatus(T(StrId::Pn532NotFound));
  } else {
    uiSetStatus(T(StrId::Ready));
  }
}

void loop() {
  displayLoop();
  delay(5);
}
