#include "ui.h"
#include "board_config.h"
#include "settings.h"
#include "strings.h"
#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Implemented in main.cpp: performs the actual PN532 read/write and calls
// uiSetStatus() / uiShowSpool() with the result.
void handleReadTagRequested();
void handleWriteTagRequested();
void handleWriteScreenOpened();

// Implemented in wifi_setup.cpp.
void wifiSetupReset();

// Implemented in main.cpp.
void handleQidiScreenOpened();
void handleQidiPrinterSelected(const String &printerId);
void handleQidiBoxSelected(int boxNumber);
void handleQidiReadBoxRequested();

static lv_obj_t *scrMain;
static lv_obj_t *lblStatus;

static lv_obj_t *scrEdit;
static lv_obj_t *ddMaterial;
static lv_obj_t *ddColor;
static lv_obj_t *lblRow3, *lblRow4;
static lv_obj_t *ddVendor;       // row 3, non-Spoolman
static lv_obj_t *ddSpool;         // row 3, Spoolman
static lv_obj_t *sbSpoolNumber; // row 4, non-Spoolman
static lv_obj_t *ddSpoolmanVendor; // row 4, Spoolman
static lv_obj_t *sbWeight;
static lv_obj_t *lblFilament, *ddFilament; // Spoolman mode, "New spool" only

static bool spoolmanModeActive = false;
static std::vector<SpoolmanVendor> uiVendors;
static std::vector<SpoolmanSpool> uiSpools;
static std::vector<SpoolmanFilament> uiFilaments;

static lv_obj_t *scrSettings;
static lv_obj_t *lblWifiStatus;
static lv_obj_t *lblPrintersSummary;
static lv_obj_t *lblSpoolman;
static lv_obj_t *swUseSpoolman;

static lv_obj_t *scrPrinters;
static lv_obj_t *ddPrinterList;
static lv_obj_t *taPrinterName;
static lv_obj_t *taPrinterAddress;
static lv_obj_t *keyboard;

static lv_obj_t *scrQidi;
static lv_obj_t *lblPrinterStatus;
static lv_obj_t *ddPrinter;
static lv_obj_t *ddBox;
static lv_obj_t *slotDot[4];
static lv_obj_t *slotLabel[4];

static void buildDropdownOptions(lv_obj_t *dd, const NamedByte *table, size_t count) {
  String options;
  for (size_t i = 0; i < count; i++) {
    if (i > 0) options += "\n";
    options += table[i].name;
  }
  lv_dropdown_set_options(dd, options.c_str());
}

static int selectedCode(lv_obj_t *dd, const NamedByte *table, size_t count) {
  uint16_t index = lv_dropdown_get_selected(dd);
  if (index >= count) return table[0].code;
  return table[index].code;
}

static void selectCode(lv_obj_t *dd, const NamedByte *table, size_t count, uint8_t code) {
  for (size_t i = 0; i < count; i++) {
    if (table[i].code == code) {
      lv_dropdown_set_selected(dd, i);
      return;
    }
  }
  lv_dropdown_set_selected(dd, 0);
}

static void readButtonCb(lv_event_t *e) { handleReadTagRequested(); }

static void writeButtonCb(lv_event_t *e) { handleWriteScreenOpened(); }

static void backButtonCb(lv_event_t *e) { lv_scr_load(scrMain); }

static void saveButtonCb(lv_event_t *e) { handleWriteTagRequested(); }

static void setNewSpoolFieldsEnabled(bool enabled) {
  auto set = [enabled](lv_obj_t *obj) {
    if (enabled) {
      lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
  };
  set(ddMaterial);
  set(ddColor);
  set(ddSpoolmanVendor);
  set(ddFilament);
}

static void filamentDropdownCb(lv_event_t *e) {
  uint16_t index = lv_dropdown_get_selected(ddFilament);
  if (index == 0 || index - 1 >= uiFilaments.size()) return; // "(pick to prefill)"

  const SpoolmanFilament &filament = uiFilaments[index - 1];
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(filament.material.c_str()));
  selectCode(ddColor, qidiColors, qidiColorsCount,
             closestQidiColorCode(filament.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == filament.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
}

static void spoolDropdownCb(lv_event_t *e) {
  uint16_t index = lv_dropdown_get_selected(ddSpool);
  if (index == 0 || index - 1 >= uiSpools.size()) {
    setNewSpoolFieldsEnabled(true);
    return;
  }
  const SpoolmanSpool &spool = uiSpools[index - 1];
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(spool.material.c_str()));
  selectCode(ddColor, qidiColors, qidiColorsCount,
             closestQidiColorCode(spool.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == spool.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
  lv_spinbox_set_value(sbWeight, spool.remainingWeightGrams);
  setNewSpoolFieldsEnabled(false);
}

static void refreshSettingsScreen() {
  String summary = String(T(StrId::Printers)) + ": ";
  if (settings.printers.empty()) {
    summary += T(StrId::NoPrinters);
  } else {
    summary += String(settings.printers.size());
    Printer *active = settings.activePrinter();
    if (active) summary += " (" + active->name + " @ " + active->address + ")";
  }
  lv_label_set_text(lblPrintersSummary, summary.c_str());

  lv_label_set_text(lblSpoolman,
                     (String(T(StrId::SpoolmanAddress)) + ": " +
                      (settings.spoolmanAddress.length()
                           ? settings.spoolmanAddress
                           : String("-")))
                         .c_str());
  if (settings.useSpoolman) {
    lv_obj_add_state(swUseSpoolman, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(swUseSpoolman, LV_STATE_CHECKED);
  }
}

static void settingsButtonCb(lv_event_t *e) {
  refreshSettingsScreen();
  lv_scr_load(scrSettings);
}

static void backToSettingsCb(lv_event_t *e) {
  refreshSettingsScreen();
  lv_scr_load(scrSettings);
}

static void useSpoolmanToggleCb(lv_event_t *e) {
  settings.useSpoolman = lv_obj_has_state(swUseSpoolman, LV_STATE_CHECKED);
  settings.save();
}

static void reconfigureNetworkCb(lv_event_t *e) { wifiSetupReset(); }

static lv_obj_t *labeledSpinbox(lv_obj_t *parent, const char *labelText, int32_t maxValue, int y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, labelText);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 20, y);

  lv_obj_t *sb = lv_spinbox_create(parent);
  lv_spinbox_set_range(sb, 0, maxValue);
  lv_spinbox_set_digit_format(sb, String(maxValue).length(), 0);
  lv_obj_set_width(sb, 120);
  lv_obj_align(sb, LV_ALIGN_TOP_LEFT, 20, y + 20);
  return sb;
}

static void buildMainScreen() {
  scrMain = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrMain);
  lv_label_set_text(title, T(StrId::AppTitle));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_t *btnSettings = lv_btn_create(scrMain);
  lv_obj_set_size(btnSettings, 50, 40);
  lv_obj_align(btnSettings, LV_ALIGN_TOP_RIGHT, -10, 15);
  lv_obj_add_event_cb(btnSettings, settingsButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblGear = lv_label_create(btnSettings);
  lv_label_set_text(lblGear, LV_SYMBOL_SETTINGS);
  lv_obj_center(lblGear);

  lblStatus = lv_label_create(scrMain);
  lv_label_set_text(lblStatus, T(StrId::Ready));
  lv_obj_set_width(lblStatus, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblStatus, LV_ALIGN_TOP_MID, 0, 70);

  lv_obj_t *btnRead = lv_btn_create(scrMain);
  lv_obj_set_size(btnRead, 220, 60);
  lv_obj_align(btnRead, LV_ALIGN_CENTER, 0, -40);
  lv_obj_add_event_cb(btnRead, readButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblRead = lv_label_create(btnRead);
  lv_label_set_text(lblRead, T(StrId::ReadTag));
  lv_obj_center(lblRead);

  lv_obj_t *btnWrite = lv_btn_create(scrMain);
  lv_obj_set_size(btnWrite, 220, 60);
  lv_obj_align(btnWrite, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_event_cb(btnWrite, writeButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblWrite = lv_label_create(btnWrite);
  lv_label_set_text(lblWrite, T(StrId::WriteTag));
  lv_obj_center(lblWrite);

  lv_obj_t *btnQidi = lv_btn_create(scrMain);
  lv_obj_set_size(btnQidi, 220, 60);
  lv_obj_align(btnQidi, LV_ALIGN_CENTER, 0, 110);
  lv_obj_add_event_cb(
      btnQidi, [](lv_event_t *e) { uiShowQidiScreen(); }, LV_EVENT_CLICKED,
      nullptr);
  lv_obj_t *lblQidi = lv_label_create(btnQidi);
  lv_label_set_text(lblQidi, T(StrId::QidiData));
  lv_obj_center(lblQidi);
}

static void buildEditScreen() {
  scrEdit = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrEdit);
  lv_label_set_text(title, T(StrId::SpoolData));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *lblMat = lv_label_create(scrEdit);
  lv_label_set_text(lblMat, T(StrId::Material));
  lv_obj_align(lblMat, LV_ALIGN_TOP_LEFT, 20, 45);
  ddMaterial = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddMaterial, SCREEN_WIDTH - 40);
  lv_obj_align(ddMaterial, LV_ALIGN_TOP_LEFT, 20, 63);
  buildDropdownOptions(ddMaterial, qidiMaterials, qidiMaterialsCount);

  lv_obj_t *lblCol = lv_label_create(scrEdit);
  lv_label_set_text(lblCol, T(StrId::Color));
  lv_obj_align(lblCol, LV_ALIGN_TOP_LEFT, 20, 100);
  ddColor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddColor, SCREEN_WIDTH - 40);
  lv_obj_align(ddColor, LV_ALIGN_TOP_LEFT, 20, 118);
  buildDropdownOptions(ddColor, qidiColors, qidiColorsCount);

  // Filament (Spoolman mode, "New spool" only): picking one of the cached
  // Spoolman filaments prefills Material/Colour/Spoolman vendor below, per
  // rfid_tag_panel.dart's second dropdown. Weight is left for manual entry.
  lblFilament = lv_label_create(scrEdit);
  lv_label_set_text(lblFilament, T(StrId::Filament));
  lv_obj_align(lblFilament, LV_ALIGN_TOP_LEFT, 20, 155);
  ddFilament = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddFilament, SCREEN_WIDTH - 40);
  lv_obj_align(ddFilament, LV_ALIGN_TOP_LEFT, 20, 173);
  lv_dropdown_set_options(ddFilament, T(StrId::NoFilament));
  lv_obj_add_event_cb(ddFilament, filamentDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Row 3: Vendor (plain mode) or Spool selector (Spoolman mode) - only one
  // is visible at a time, see uiSetSpoolmanMode().
  lblRow3 = lv_label_create(scrEdit);
  lv_obj_align(lblRow3, LV_ALIGN_TOP_LEFT, 20, 210);
  ddVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddVendor, SCREEN_WIDTH - 40);
  lv_obj_align(ddVendor, LV_ALIGN_TOP_LEFT, 20, 228);
  buildDropdownOptions(ddVendor, qidiVendors, qidiVendorsCount);
  ddSpool = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpool, SCREEN_WIDTH - 40);
  lv_obj_align(ddSpool, LV_ALIGN_TOP_LEFT, 20, 228);
  lv_dropdown_set_options(ddSpool, T(StrId::NewSpool));
  lv_obj_add_event_cb(ddSpool, spoolDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Row 4: manual spool number (plain mode) or Spoolman vendor (Spoolman
  // mode, only used/editable for a new spool).
  lblRow4 = lv_label_create(scrEdit);
  lv_obj_align(lblRow4, LV_ALIGN_TOP_LEFT, 20, 265);
  sbSpoolNumber = lv_spinbox_create(scrEdit);
  lv_spinbox_set_range(sbSpoolNumber, 0, 999);
  lv_spinbox_set_digit_format(sbSpoolNumber, 3, 0);
  lv_obj_set_width(sbSpoolNumber, 120);
  lv_obj_align(sbSpoolNumber, LV_ALIGN_TOP_LEFT, 20, 283);
  ddSpoolmanVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpoolmanVendor, SCREEN_WIDTH - 40);
  lv_obj_align(ddSpoolmanVendor, LV_ALIGN_TOP_LEFT, 20, 283);

  sbWeight = labeledSpinbox(scrEdit, T(StrId::Weight), 65535, 328);

  lv_obj_t *btnSave = lv_btn_create(scrEdit);
  lv_obj_set_size(btnSave, 200, 55);
  lv_obj_align(btnSave, LV_ALIGN_BOTTOM_LEFT, 20, -20);
  lv_obj_add_event_cb(btnSave, saveButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblSave = lv_label_create(btnSave);
  lv_label_set_text(lblSave, T(StrId::WriteTag));
  lv_obj_center(lblSave);

  lv_obj_t *btnBack = lv_btn_create(scrEdit);
  lv_obj_set_size(btnBack, 120, 55);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
  lv_obj_add_event_cb(btnBack, backButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, T(StrId::Back));
  lv_obj_center(lblBack);

  uiSetSpoolmanMode(false);
}

static void buildSettingsScreen() {
  scrSettings = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrSettings);
  lv_label_set_text(title, T(StrId::Settings));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lblWifiStatus = lv_label_create(scrSettings);
  lv_obj_set_width(lblWifiStatus, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblWifiStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblWifiStatus, LV_ALIGN_TOP_LEFT, 20, 50);

  lblPrintersSummary = lv_label_create(scrSettings);
  lv_obj_set_width(lblPrintersSummary, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblPrintersSummary, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblPrintersSummary, LV_ALIGN_TOP_LEFT, 20, 90);

  lv_obj_t *btnManagePrinters = lv_btn_create(scrSettings);
  lv_obj_set_size(btnManagePrinters, SCREEN_WIDTH - 40, 45);
  lv_obj_align(btnManagePrinters, LV_ALIGN_TOP_LEFT, 20, 125);
  lv_obj_add_event_cb(
      btnManagePrinters, [](lv_event_t *e) { lv_scr_load(scrPrinters); },
      LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblManagePrinters = lv_label_create(btnManagePrinters);
  lv_label_set_text(lblManagePrinters, T(StrId::ManagePrinters));
  lv_obj_center(lblManagePrinters);

  lblSpoolman = lv_label_create(scrSettings);
  lv_obj_set_width(lblSpoolman, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblSpoolman, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblSpoolman, LV_ALIGN_TOP_LEFT, 20, 185);

  lv_obj_t *lblUseSpoolman = lv_label_create(scrSettings);
  lv_label_set_text(lblUseSpoolman, T(StrId::UseSpoolman));
  lv_obj_align(lblUseSpoolman, LV_ALIGN_TOP_LEFT, 20, 225);
  swUseSpoolman = lv_switch_create(scrSettings);
  lv_obj_align(swUseSpoolman, LV_ALIGN_TOP_LEFT, 220, 220);
  lv_obj_add_event_cb(swUseSpoolman, useSpoolmanToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *btnReconfigure = lv_btn_create(scrSettings);
  lv_obj_set_size(btnReconfigure, SCREEN_WIDTH - 40, 55);
  lv_obj_align(btnReconfigure, LV_ALIGN_BOTTOM_MID, 0, -90);
  lv_obj_add_event_cb(btnReconfigure, reconfigureNetworkCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblReconfigure = lv_label_create(btnReconfigure);
  lv_label_set_text(lblReconfigure, T(StrId::ReconfigureNetwork));
  lv_obj_center(lblReconfigure);

  lv_obj_t *btnBack = lv_btn_create(scrSettings);
  lv_obj_set_size(btnBack, 120, 55);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btnBack, backButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, T(StrId::Back));
  lv_obj_center(lblBack);
}

static void refreshPrinterDropdown() {
  String options;
  for (size_t i = 0; i < settings.printers.size(); i++) {
    if (i > 0) options += "\n";
    options += settings.printers[i].name.length() ? settings.printers[i].name
                                                    : settings.printers[i].id;
  }
  if (options.length() == 0) options = T(StrId::NoPrinters);
  lv_dropdown_set_options(ddPrinterList, options.c_str());
}

static void clearPrinterFields() {
  lv_textarea_set_text(taPrinterName, "");
  lv_textarea_set_text(taPrinterAddress, "");
}

static void printerListDropdownCb(lv_event_t *e) {
  uint16_t index = lv_dropdown_get_selected(ddPrinterList);
  if (index >= settings.printers.size()) {
    clearPrinterFields();
    return;
  }
  lv_textarea_set_text(taPrinterName, settings.printers[index].name.c_str());
  lv_textarea_set_text(taPrinterAddress, settings.printers[index].address.c_str());
}

static void newPrinterCb(lv_event_t *e) {
  lv_dropdown_set_selected(ddPrinterList, settings.printers.size());
  clearPrinterFields();
}

static void savePrinterCb(lv_event_t *e) {
  const char *name = lv_textarea_get_text(taPrinterName);
  const char *address = lv_textarea_get_text(taPrinterAddress);
  if (strlen(address) == 0) return; // an address is the minimum needed to be useful

  uint16_t index = lv_dropdown_get_selected(ddPrinterList);
  if (index < settings.printers.size()) {
    settings.printers[index].name = name;
    settings.printers[index].address = address;
  } else {
    settings.addPrinter(name, address);
  }
  settings.save();
  refreshPrinterDropdown();
  refreshSettingsScreen();
}

static void deletePrinterCb(lv_event_t *e) {
  uint16_t index = lv_dropdown_get_selected(ddPrinterList);
  if (index >= settings.printers.size()) return;
  settings.removePrinter(settings.printers[index].id);
  settings.save();
  refreshPrinterDropdown();
  lv_dropdown_set_selected(ddPrinterList, 0);
  printerListDropdownCb(nullptr);
  refreshSettingsScreen();
}

static void textareaFocusCb(lv_event_t *e) {
  lv_obj_t *textarea = (lv_obj_t *)lv_event_get_target(e);
  lv_keyboard_set_textarea(keyboard, textarea);
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void textareaDefocusCb(lv_event_t *e) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void buildPrintersScreen() {
  scrPrinters = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrPrinters);
  lv_label_set_text(title, T(StrId::Printers));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

  ddPrinterList = lv_dropdown_create(scrPrinters);
  lv_obj_set_width(ddPrinterList, SCREEN_WIDTH - 40);
  lv_obj_align(ddPrinterList, LV_ALIGN_TOP_LEFT, 20, 35);
  lv_obj_add_event_cb(ddPrinterList, printerListDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *lblName = lv_label_create(scrPrinters);
  lv_label_set_text(lblName, T(StrId::PrinterName));
  lv_obj_align(lblName, LV_ALIGN_TOP_LEFT, 20, 75);
  taPrinterName = lv_textarea_create(scrPrinters);
  lv_textarea_set_one_line(taPrinterName, true);
  lv_obj_set_width(taPrinterName, SCREEN_WIDTH - 40);
  lv_obj_align(taPrinterName, LV_ALIGN_TOP_LEFT, 20, 95);
  lv_obj_add_event_cb(taPrinterName, textareaFocusCb, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(taPrinterName, textareaDefocusCb, LV_EVENT_DEFOCUSED, nullptr);

  lv_obj_t *lblAddress = lv_label_create(scrPrinters);
  lv_label_set_text(lblAddress, T(StrId::MoonrakerAddress));
  lv_obj_align(lblAddress, LV_ALIGN_TOP_LEFT, 20, 135);
  taPrinterAddress = lv_textarea_create(scrPrinters);
  lv_textarea_set_one_line(taPrinterAddress, true);
  lv_obj_set_width(taPrinterAddress, SCREEN_WIDTH - 40);
  lv_obj_align(taPrinterAddress, LV_ALIGN_TOP_LEFT, 20, 155);
  lv_obj_add_event_cb(taPrinterAddress, textareaFocusCb, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(taPrinterAddress, textareaDefocusCb, LV_EVENT_DEFOCUSED, nullptr);

  lv_obj_t *btnNew = lv_btn_create(scrPrinters);
  lv_obj_set_size(btnNew, 90, 45);
  lv_obj_align(btnNew, LV_ALIGN_TOP_LEFT, 20, 195);
  lv_obj_add_event_cb(btnNew, newPrinterCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblNew = lv_label_create(btnNew);
  lv_label_set_text(lblNew, T(StrId::New));
  lv_obj_center(lblNew);

  lv_obj_t *btnSave = lv_btn_create(scrPrinters);
  lv_obj_set_size(btnSave, 90, 45);
  lv_obj_align(btnSave, LV_ALIGN_TOP_LEFT, 120, 195);
  lv_obj_add_event_cb(btnSave, savePrinterCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblSavePrinter = lv_label_create(btnSave);
  lv_label_set_text(lblSavePrinter, T(StrId::Save));
  lv_obj_center(lblSavePrinter);

  lv_obj_t *btnDelete = lv_btn_create(scrPrinters);
  lv_obj_set_size(btnDelete, 90, 45);
  lv_obj_align(btnDelete, LV_ALIGN_TOP_LEFT, 220, 195);
  lv_obj_add_event_cb(btnDelete, deletePrinterCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblDelete = lv_label_create(btnDelete);
  lv_label_set_text(lblDelete, T(StrId::Delete));
  lv_obj_center(lblDelete);

  lv_obj_t *btnBack = lv_btn_create(scrPrinters);
  lv_obj_set_size(btnBack, 120, 40);
  lv_obj_align(btnBack, LV_ALIGN_TOP_RIGHT, -20, 195);
  lv_obj_add_event_cb(btnBack, backToSettingsCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBackPrinters = lv_label_create(btnBack);
  lv_label_set_text(lblBackPrinters, T(StrId::Back));
  lv_obj_center(lblBackPrinters);

  keyboard = lv_keyboard_create(scrPrinters);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void boxDropdownCb(lv_event_t *e) {
  handleQidiBoxSelected(lv_dropdown_get_selected(ddBox) + 1);
}

static void readBoxButtonCb(lv_event_t *e) { handleQidiReadBoxRequested(); }

static uint32_t colorFor(bool loaded, bool active) {
  if (active) return 0x00CFEA; // cyan: feeding
  if (loaded) return 0x2ECC71; // green: loaded
  return 0x888888;             // grey: empty
}

static void buildQidiScreen() {
  scrQidi = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrQidi);
  lv_label_set_text(title, T(StrId::QidiData));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *lblPrinter = lv_label_create(scrQidi);
  lv_label_set_text(lblPrinter, T(StrId::Printer));
  lv_obj_align(lblPrinter, LV_ALIGN_TOP_LEFT, 20, 40);
  ddPrinter = lv_dropdown_create(scrQidi);
  lv_obj_set_width(ddPrinter, SCREEN_WIDTH - 40);
  lv_obj_align(ddPrinter, LV_ALIGN_TOP_LEFT, 20, 58);
  lv_dropdown_set_options(ddPrinter, T(StrId::NoPrinters));
  lv_obj_add_event_cb(
      ddPrinter,
      [](lv_event_t *e) {
        uint16_t index = lv_dropdown_get_selected(ddPrinter);
        if (index < settings.printers.size()) {
          handleQidiPrinterSelected(settings.printers[index].id);
        }
      },
      LV_EVENT_VALUE_CHANGED, nullptr);

  lblPrinterStatus = lv_label_create(scrQidi);
  lv_obj_set_width(lblPrinterStatus, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblPrinterStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblPrinterStatus, LV_ALIGN_TOP_LEFT, 20, 95);

  lv_obj_t *lblBox = lv_label_create(scrQidi);
  lv_label_set_text(lblBox, T(StrId::Box));
  lv_obj_align(lblBox, LV_ALIGN_TOP_LEFT, 20, 130);
  ddBox = lv_dropdown_create(scrQidi);
  lv_obj_set_width(ddBox, 100);
  lv_obj_align(ddBox, LV_ALIGN_TOP_LEFT, 90, 125);
  lv_dropdown_set_options(ddBox, "1");
  lv_obj_add_event_cb(ddBox, boxDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *btnReadBox = lv_btn_create(scrQidi);
  lv_obj_set_size(btnReadBox, 130, 40);
  lv_obj_align(btnReadBox, LV_ALIGN_TOP_RIGHT, -20, 122);
  lv_obj_add_event_cb(btnReadBox, readBoxButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblReadBox = lv_label_create(btnReadBox);
  lv_label_set_text(lblReadBox, T(StrId::ReadBox));
  lv_obj_center(lblReadBox);

  int y = 175;
  const int rowHeight = 58;
  for (int i = 0; i < 4; i++) {
    slotDot[i] = lv_obj_create(scrQidi);
    lv_obj_set_size(slotDot[i], 20, 20);
    lv_obj_set_style_radius(slotDot[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(slotDot[i], 0, 0);
    lv_obj_align(slotDot[i], LV_ALIGN_TOP_LEFT, 20, y + 5);

    slotLabel[i] = lv_label_create(scrQidi);
    lv_obj_set_width(slotLabel[i], SCREEN_WIDTH - 90);
    lv_label_set_long_mode(slotLabel[i], LV_LABEL_LONG_WRAP);
    lv_obj_align(slotLabel[i], LV_ALIGN_TOP_LEFT, 55, y);

    y += rowHeight;
  }

  lv_obj_t *btnBack = lv_btn_create(scrQidi);
  lv_obj_set_size(btnBack, 120, 55);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btnBack, backButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, T(StrId::Back));
  lv_obj_center(lblBack);
}

void uiInit() {
  buildMainScreen();
  buildEditScreen();
  buildSettingsScreen();
  buildPrintersScreen();
  buildQidiScreen();
  lv_label_set_text(lblWifiStatus, T(StrId::WifiStatus));
  lv_scr_load(scrMain);
}

void uiSetStatus(const char *text) {
  lv_label_set_text(lblStatus, text);
}

void uiSetWifiStatus(const char *text) {
  lv_label_set_text(lblWifiStatus, text);
}

void uiShowSpool(const FilamentSpool &spool) {
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount, spool.materialCode);
  selectCode(ddColor, qidiColors, qidiColorsCount, spool.colorCode);
  selectCode(ddVendor, qidiVendors, qidiVendorsCount,
             spool.internalVendorId != 0 ? qidiVendorCode("GENERIC") : spool.vendorCode);
  lv_spinbox_set_value(sbSpoolNumber, spool.spoolNumber);
  lv_spinbox_set_value(sbWeight, spool.lastWeightGrams);

  if (spoolmanModeActive) {
    // Point the Spool selector at the matching cached spool, if any, so
    // writing back does not create a duplicate; otherwise leave "New spool"
    // selected with the tag's own fields as a manual fallback.
    lv_dropdown_set_selected(ddSpool, 0);
    setNewSpoolFieldsEnabled(true);
    for (size_t i = 0; i < uiSpools.size(); i++) {
      if (uiSpools[i].id == spool.spoolNumber) {
        lv_dropdown_set_selected(ddSpool, i + 1);
        spoolDropdownCb(nullptr);
        break;
      }
    }
  }

  lv_scr_load(scrEdit);
}

void uiShowEditScreenForWrite() {
  if (spoolmanModeActive) {
    lv_dropdown_set_selected(ddSpool, 0);
    setNewSpoolFieldsEnabled(true);
  }
  lv_scr_load(scrEdit);
}

void uiSetSpoolmanMode(bool active) {
  spoolmanModeActive = active;
  lv_label_set_text(lblRow3, active ? T(StrId::Spool) : T(StrId::Vendor));
  lv_label_set_text(lblRow4, active ? T(StrId::SpoolmanVendor)
                                     : (String(T(StrId::SpoolNumber)) + " (0-999)").c_str());

  auto show = [](lv_obj_t *obj, bool visible) {
    if (visible) {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
  };
  show(ddVendor, !active);
  show(ddSpool, active);
  show(sbSpoolNumber, !active);
  show(ddSpoolmanVendor, active);
  show(lblFilament, active);
  show(ddFilament, active);
}

void uiSetSpoolmanLists(const std::vector<SpoolmanVendor> &vendors,
                        const std::vector<SpoolmanSpool> &spools,
                        const std::vector<SpoolmanFilament> &filaments) {
  uiVendors = vendors;
  uiSpools = spools;
  uiFilaments = filaments;

  String vendorOptions;
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (i > 0) vendorOptions += "\n";
    vendorOptions += uiVendors[i].name;
  }
  if (vendorOptions.length() == 0) vendorOptions = "-";
  lv_dropdown_set_options(ddSpoolmanVendor, vendorOptions.c_str());

  String spoolOptions = T(StrId::NewSpool);
  for (const SpoolmanSpool &spool : uiSpools) {
    spoolOptions += "\n#" + String(spool.id) + " " + spool.material;
    if (spool.vendorName.length()) spoolOptions += " (" + spool.vendorName + ")";
  }
  lv_dropdown_set_options(ddSpool, spoolOptions.c_str());
  lv_dropdown_set_selected(ddSpool, 0);

  String filamentOptions = T(StrId::NoFilament);
  for (const SpoolmanFilament &filament : uiFilaments) {
    filamentOptions += "\n" + filament.vendorName + " " + filament.material;
    if (filament.colorHex.length()) filamentOptions += " " + filament.colorHex;
  }
  lv_dropdown_set_options(ddFilament, filamentOptions.c_str());
  lv_dropdown_set_selected(ddFilament, 0);
}

bool uiIsNewSpoolSelected() {
  return !spoolmanModeActive || lv_dropdown_get_selected(ddSpool) == 0;
}

int uiSelectedExistingSpoolId() {
  if (uiIsNewSpoolSelected()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddSpool);
  if (index - 1 >= uiSpools.size()) return -1;
  return uiSpools[index - 1].id;
}

int uiSelectedSpoolmanVendorId() {
  if (!spoolmanModeActive || uiVendors.empty()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddSpoolmanVendor);
  if (index >= uiVendors.size()) return -1;
  return uiVendors[index].id;
}

void uiShowQidiScreen() {
  lv_scr_load(scrQidi);
  handleQidiScreenOpened();
}

void uiSetQidiPrinterStatus(const char *text) {
  lv_label_set_text(lblPrinterStatus, text);
}

void uiSetQidiPrinterList(const std::vector<Printer> &printers, const String &selectedId) {
  String options;
  int selectedIndex = 0;
  for (size_t i = 0; i < printers.size(); i++) {
    if (i > 0) options += "\n";
    options += printers[i].name.length() ? printers[i].name : printers[i].id;
    if (printers[i].id == selectedId) selectedIndex = i;
  }
  if (options.length() == 0) options = T(StrId::NoPrinters);
  lv_dropdown_set_options(ddPrinter, options.c_str());
  if (!printers.empty()) lv_dropdown_set_selected(ddPrinter, selectedIndex);
}

void uiSetQidiBoxCount(int boxCount) {
  if (boxCount <= 0) {
    lv_dropdown_set_options(ddBox, "-");
    for (int i = 0; i < 4; i++) {
      lv_obj_set_style_bg_color(slotDot[i], lv_color_hex(colorFor(false, false)), 0);
      lv_label_set_text(slotLabel[i], T(StrId::SlotEmpty));
    }
    return;
  }
  String options;
  for (int i = 1; i <= boxCount; i++) {
    if (i > 1) options += "\n";
    options += String(i);
  }
  lv_dropdown_set_options(ddBox, options.c_str());
  lv_dropdown_set_selected(ddBox, 0);
}

void uiSetQidiSlots(const QidiSlot slots[4]) {
  for (int i = 0; i < 4; i++) {
    const QidiSlot &slot = slots[i];
    lv_obj_set_style_bg_color(slotDot[i], lv_color_hex(colorFor(slot.loaded, slot.active)), 0);

    if (!slot.loaded) {
      String text = "Slot " + String(i) + ": " + T(StrId::SlotEmpty);
      lv_label_set_text(slotLabel[i], text.c_str());
      continue;
    }

    String text = "Slot " + String(i) + ": " + slot.material;
    if (slot.vendor.length()) text += " (" + slot.vendor + ")";
    if (slot.colorHex.length()) text += " " + slot.colorHex;
    if (slot.spoolNumber >= 0) text += " #" + String(slot.spoolNumber);
    if (slot.spoolmanVendorName.length()) text += " - " + slot.spoolmanVendorName;
    if (slot.weightGrams >= 0) text += " - " + String(slot.weightGrams) + "g";
    lv_label_set_text(slotLabel[i], text.c_str());
  }
}

FilamentSpool uiCurrentSpool() {
  FilamentSpool spool;
  spool.materialCode = selectedCode(ddMaterial, qidiMaterials, qidiMaterialsCount);
  spool.colorCode = selectedCode(ddColor, qidiColors, qidiColorsCount);
  spool.lastWeightGrams = (uint16_t)lv_spinbox_get_value(sbWeight);

  if (spoolmanModeActive) {
    // spoolNumber/internalVendorId/vendorCode are resolved by main.cpp
    // (existing spool id, or a newly created one) after this call.
    spool.vendorCode = qidiVendorCode("GENERIC");
    spool.internalVendorId = 0;
    spool.spoolNumber = 0;
  } else {
    spool.vendorCode = selectedCode(ddVendor, qidiVendors, qidiVendorsCount);
    spool.internalVendorId = 0;
    spool.spoolNumber = (uint16_t)lv_spinbox_get_value(sbSpoolNumber);
  }
  return spool;
}
