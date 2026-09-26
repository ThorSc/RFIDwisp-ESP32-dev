#include "wifi_setup.h"
#include "settings.h"
#include <WiFiManager.h>

static bool portalRanThisBoot = false;

static void onSaveConfig() { portalRanThisBoot = true; }

bool wifiSetupConnect() {
  WiFiManager wm;
  // wm.setDebugOutput(false); // uncomment to quiet WiFiManager's serial log

  // The portal only ever sets up the first printer (for a friction-free
  // first boot without on-screen typing); additional printers are added
  // from the on-device Settings screen. Prefilled with the current values
  // so re-running the portal just for Wi-Fi doesn't blank them out.
  Printer *existing = settings.activePrinter();
  WiFiManagerParameter customPrinter(
      "printer", "Printer name", existing ? existing->name.c_str() : "", 40);
  WiFiManagerParameter customMoonraker(
      "moonraker", "Moonraker address (http://ip:7125)",
      existing ? existing->address.c_str() : "", 64);
  WiFiManagerParameter customSpoolman(
      "spoolman", "Spoolman address (http://ip:7912, leave empty to skip)",
      settings.spoolmanAddress.c_str(), 64);

  wm.addParameter(&customPrinter);
  wm.addParameter(&customMoonraker);
  wm.addParameter(&customSpoolman);
  wm.setSaveConfigCallback(onSaveConfig);
  wm.setConfigPortalTimeout(300); // give up and retry later rather than hang forever

  bool connected = wm.autoConnect("RFIDwisp-Setup");

  if (portalRanThisBoot) {
    Printer *printer = settings.activePrinter();
    if (printer) {
      printer->name = customPrinter.getValue();
      printer->address = customMoonraker.getValue();
    } else if (strlen(customMoonraker.getValue()) > 0) {
      settings.addPrinter(customPrinter.getValue(), customMoonraker.getValue());
    }
    settings.spoolmanAddress = customSpoolman.getValue();
    settings.useSpoolman = settings.spoolmanAddress.length() > 0;
    settings.save();
  }

  return connected;
}

void wifiSetupReset() {
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}
