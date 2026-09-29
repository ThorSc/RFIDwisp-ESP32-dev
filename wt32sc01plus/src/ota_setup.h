#pragma once

// Wi-Fi OTA firmware updates (Arduino/PlatformIO "network port"), so a
// release can be flashed without a USB cable once the device is on the same
// network. Advertises itself under wifi_setup.h's kNetworkHostname
// ("RFIDwisp-mobile.local"), optionally protected by settings.otaPassword.
//
// Call otaSetup() once after a successful Wi-Fi connection, then otaLoop()
// on every loop() iteration.
void otaSetup();
void otaLoop();
