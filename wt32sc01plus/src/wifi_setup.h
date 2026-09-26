#pragma once

// Connects to the saved Wi-Fi network. If there is none (first boot) or the
// saved one cannot be reached, opens a "RFIDwisp-Setup" access point with a
// captive portal: the phone/laptop that joins it is redirected to a web form
// for the Wi-Fi credentials plus the Moonraker/Spoolman addresses and printer
// name, which are then saved to flash (AppSettings, see settings.h).
//
// Blocks until connected or the user leaves the portal after saving.
// Returns false only if the portal itself could not be started.
bool wifiSetupConnect();

// Erases the saved Wi-Fi credentials and reboots into the captive portal.
// Call from the on-device Settings screen's "Reconfigure network" button.
void wifiSetupReset();
