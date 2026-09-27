#include "wifi_setup.h"
#include "settings.h"
#include <WiFiManager.h>

static bool portalRanThisBoot = false;

static void onSaveConfig() { portalRanThisBoot = true; }

bool wifiSetupConnect() {
  WiFiManager wm;

  WiFiManagerParameter customSpoolman(
      "spoolman", "Spoolman address (http://ip:7912, leave empty to skip)",
      settings.spoolmanAddress.c_str(), 64);

  wm.addParameter(&customSpoolman);
  wm.setSaveConfigCallback(onSaveConfig);
  wm.setConfigPortalTimeout(300); // give up and retry later rather than hang forever

  bool connected = wm.autoConnect("RFIDwisp-Setup");

  if (portalRanThisBoot) {
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
