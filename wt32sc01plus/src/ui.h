#pragma once
#include "moonraker_client.h"
#include "qidi_tag.h"
#include "settings.h"
#include "spoolman_client.h"
#include <vector>

// Called once at startup, after displaySetup().
void uiInit();

// Fills the edit screen's fields from a spool just read off a tag and shows
// that screen. If a cached Spoolman spool matches spool.spoolNumber, the
// Spool selector is pointed at it instead of "New spool".
void uiShowSpool(const FilamentSpool &spool);

// Shows the edit screen for writing a new tag (blank fields, "New spool"
// selected). Called by main.cpp's handleWriteScreenOpened after refreshing
// the Spoolman caches (if Spoolman is active).
void uiShowEditScreenForWrite();

// Whether the edit screen currently shows the Spoolman-integrated fields
// (Spool selector + Spoolman vendor) instead of the plain QIDI vendor +
// manual spool number fields.
void uiSetSpoolmanMode(bool active);

// The cached Spoolman vendors/spools/filaments to show in the edit screen's
// dropdowns (Spool, Spoolman vendor, and Filament - the last one lets a new
// spool's Material/Colour/Vendor be prefilled from an existing filament,
// mirroring rfid_tag_panel.dart's second dropdown).
void uiSetSpoolmanLists(const std::vector<SpoolmanVendor> &vendors,
                        const std::vector<SpoolmanSpool> &spools,
                        const std::vector<SpoolmanFilament> &filaments);

// True if "New spool" is selected on the edit screen (Spoolman mode only).
bool uiIsNewSpoolSelected();

// The id of the Spoolman spool selected on the edit screen; -1 if "New
// spool" is selected or Spoolman mode is off.
int uiSelectedExistingSpoolId();

// The Spoolman vendor id chosen for a new spool (Spoolman mode, "New spool"
// selected); -1 if none of the cached vendors is selected.
int uiSelectedSpoolmanVendorId();

// Updates the status line on the main screen.
void uiSetStatus(const char *text);

// Updates the Wi-Fi status line shown on the settings screen.
void uiSetWifiStatus(const char *text);

// Shows the QIDI Data screen and asks main.cpp (via handleQidiScreenOpened,
// implemented there) to check the printer and load box 1.
void uiShowQidiScreen();

// Updates the printer connection status line on the QIDI Data screen.
void uiSetQidiPrinterStatus(const char *text);

// Fills the printer selector; selectedId picks which one shows as current.
void uiSetQidiPrinterList(const std::vector<Printer> &printers, const String &selectedId);

// Fills the box selector (1..boxCount); boxCount 0 shows no boxes at all.
void uiSetQidiBoxCount(int boxCount);

// Updates the 4 visible slot rows for the currently selected box.
void uiSetQidiSlots(const QidiSlot slots[4]);

// Reads the edit screen's fields back into a spool, for writing to a tag.
FilamentSpool uiCurrentSpool();
