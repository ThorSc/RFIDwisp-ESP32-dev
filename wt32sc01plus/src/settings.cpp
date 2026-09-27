#include "settings.h"
#include <Preferences.h>

AppSettings settings;

static const char *kNamespace = "rfidwisp";

void AppSettings::load() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  spoolmanAddress = prefs.getString("spoolmanAddr", "");
  useSpoolman = prefs.getBool("useSpoolman", false);
  prefs.end();
}

void AppSettings::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("spoolmanAddr", spoolmanAddress);
  prefs.putBool("useSpoolman", useSpoolman);
  prefs.end();
}
