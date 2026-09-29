#include "ota_setup.h"
#include <ArduinoOTA.h>
#include "settings.h"
#include "strings.h"
#include "ui.h"
#include "wifi_setup.h"

void otaSetup() {
  ArduinoOTA.setHostname(kNetworkHostname);
  if (settings.otaPassword.length() > 0) {
    ArduinoOTA.setPassword(settings.otaPassword.c_str());
  }

  ArduinoOTA.onStart([]() { uiSetStatus(T(StrId::OtaUpdating)); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) return;
    unsigned int percent = (progress * 100) / total;
    uiSetStatus((String(T(StrId::OtaUpdating)) + " " + String(percent) + "%").c_str());
  });
  ArduinoOTA.onError([](ota_error_t error) { uiSetStatus(T(StrId::OtaUpdateFailed)); });

  ArduinoOTA.begin();
}

void otaLoop() { ArduinoOTA.handle(); }
