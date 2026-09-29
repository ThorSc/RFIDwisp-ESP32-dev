#include "settings.h"
#include <Preferences.h>

AppSettings settings;

static const char *kNamespace = "rfidwisp";

void AppSettings::load() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  spoolmanAddress = prefs.getString("spoolmanAddr", "");
  useSpoolman = prefs.getBool("useSpoolman", false);
  sleepMinutes = min<uint8_t>(prefs.getUChar("sleepMin", 10), 60);
  otaPassword = prefs.getString("otaPass", "");
  prefs.end();
}

void AppSettings::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("spoolmanAddr", spoolmanAddress);
  prefs.putBool("useSpoolman", useSpoolman);
  prefs.putUChar("sleepMin", sleepMinutes);
  prefs.putString("otaPass", otaPassword);
  prefs.end();
}
