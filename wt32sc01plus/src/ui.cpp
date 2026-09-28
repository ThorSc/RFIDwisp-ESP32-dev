#include "ui.h"
#include "display_setup.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#include "board_config.h"
#include "settings.h"
#include "strings.h"
#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Implemented in main.cpp: performs the actual RC522 read/write and calls
// uiSetStatus() / uiShowSpool() with the result.
void handleReadTagRequested();
void handleWriteTagRequested();
void handleWriteScreenOpened();

// Implemented in wifi_setup.cpp.
void wifiSetupReset();

static lv_obj_t *scrMain;
static lv_obj_t *lblStatus;

static lv_obj_t *scrEdit;
static lv_obj_t *ddMaterial;
static lv_obj_t *ddColor;
static lv_obj_t *lblRow3, *lblSpoolRow;
static lv_obj_t *ddVendor;       // row 3, non-Spoolman
static lv_obj_t *ddSpool;         // row 3, Spoolman
static lv_obj_t *sbSpoolNumber; // row 4, non-Spoolman
static lv_obj_t *ddSpoolmanVendor; // row 4, Spoolman
static lv_obj_t *sbWeight;
static lv_obj_t *lblFilament, *ddFilament; // Spoolman mode, "New spool" only
static lv_obj_t *btnColor, *lblColorHex; // shows the selected colour; ddColor is hidden state
static lv_obj_t *colorOverlay;
static lv_obj_t *lblEditStatus;
static lv_obj_t *sbSleep; // settings: screen sleep timeout in minutes
static lv_obj_t *btnSpool, *spoolSwatch, *spoolBtnLabel;          // Spoolman mode; ddSpool is hidden state
static lv_obj_t *btnFilament, *filamentSwatch, *filamentBtnLabel; // ditto for ddFilament
static lv_obj_t *pickerOverlay;
static bool pickerForSpool;

static bool spoolmanModeActive = false;
static std::vector<SpoolmanVendor> uiVendors;
static std::vector<SpoolmanSpool> uiSpools;
static std::vector<SpoolmanFilament> uiFilaments;

static lv_obj_t *scrSettings;
static lv_obj_t *lblWifiStatus;
static lv_obj_t *taSpoolmanAddress;
static lv_obj_t *keyboard;
static lv_obj_t *swUseSpoolman;



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

static uint32_t rgbFromHex(const char *hex) {
  if (hex[0] == '#') hex++;
  return (uint32_t)strtoul(hex, nullptr, 16);
}

static lv_color_t contrastingText(uint32_t rgb) {
  uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  return (299 * r + 587 * g + 114 * b) > 128000 ? lv_color_black() : lv_color_white();
}

static void updateColorButton() {
  uint16_t index = lv_dropdown_get_selected(ddColor);
  if (index >= qidiColorsCount) index = 0;
  uint32_t rgb = rgbFromHex(qidiColors[index].name);
  lv_obj_set_style_bg_color(btnColor, lv_color_hex(rgb), 0);
  lv_obj_set_style_text_color(lblColorHex, contrastingText(rgb), 0);
  lv_label_set_text(lblColorHex, qidiColors[index].name);
}

static void selectColorCode(uint8_t code) {
  selectCode(ddColor, qidiColors, qidiColorsCount, code);
  updateColorButton();
}

static void closeColorOverlay() {
  if (colorOverlay) {
    lv_obj_del_async(colorOverlay);
    colorOverlay = nullptr;
  }
}

static void colorSwatchCb(lv_event_t *e) {
  uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
  lv_dropdown_set_selected(ddColor, (uint16_t)index);
  updateColorButton();
  closeColorOverlay();
}

static void openColorOverlay(lv_event_t *e) {
  if (colorOverlay) return;
  colorOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(colorOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(colorOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(colorOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(colorOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(colorOverlay, 0, 0);
  lv_obj_set_style_radius(colorOverlay, 0, 0);
  lv_obj_set_style_pad_all(colorOverlay, 0, 0);
  lv_obj_clear_flag(colorOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      colorOverlay,
      [](lv_event_t *ev) {
        if (lv_event_get_target(ev) == lv_event_get_current_target(ev)) closeColorOverlay();
      },
      LV_EVENT_CLICKED, nullptr);

  const int cols = 6, cellW = 70, cellH = 64;
  const int originX = (SCREEN_WIDTH - cols * cellW) / 2;
  const int rows = (qidiColorsCount + cols - 1) / cols;
  const int originY = (SCREEN_HEIGHT - rows * cellH) / 2;
  for (size_t i = 0; i < qidiColorsCount; i++) {
    uint32_t rgb = rgbFromHex(qidiColors[i].name);
    lv_obj_t *btn = lv_btn_create(colorOverlay);
    lv_obj_set_size(btn, cellW - 8, cellH - 8);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, originX + (i % cols) * cellW + 4,
                 originY + (i / cols) * cellH + 4);
    lv_obj_set_style_bg_color(btn, lv_color_hex(rgb), 0);
    lv_obj_add_event_cb(btn, colorSwatchCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, qidiColors[i].name);
    lv_obj_set_style_text_color(label, contrastingText(rgb), 0);
    lv_obj_center(label);
  }
}

static void readButtonCb(lv_event_t *e) { handleReadTagRequested(); }

static void writeButtonCb(lv_event_t *e) { handleWriteScreenOpened(); }

static void backButtonCb(lv_event_t *e) { lv_scr_load(scrMain); }

static void saveButtonCb(lv_event_t *e) { handleWriteTagRequested(); }

static void setNumberField(lv_obj_t *field, long value) {
  lv_textarea_set_text(field, String(value).c_str());
}

static long getNumberField(lv_obj_t *field) {
  return atol(lv_textarea_get_text(field));
}

static lv_obj_t *numberOverlay, *numberInput, *numberTarget;
static long numberMax;

static void closeNumberOverlay() {
  if (numberOverlay) {
    lv_obj_del_async(numberOverlay);
    numberOverlay = nullptr;
    numberTarget = nullptr;
  }
}

static void numberKeyboardCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *text = lv_textarea_get_text(numberInput);
    if (text[0] != '\0' && numberTarget) {
      long value = atol(text);
      if (value > numberMax) value = numberMax;
      setNumberField(numberTarget, value);
      if (numberTarget == sbSleep) {
        settings.sleepMinutes = (uint8_t)value;
        settings.save();
        displaySetSleepTimeout(settings.sleepMinutes);
      }
    }
    closeNumberOverlay();
  } else if (code == LV_EVENT_CANCEL) {
    closeNumberOverlay();
  }
}

// Tapping a number field opens a full-screen numeric keypad (a spinbox has no
// keyboard, and the normal keyboard would cover the fields at the bottom).
static void openNumberOverlay(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
  if (numberOverlay || lv_obj_has_state(target, LV_STATE_DISABLED)) return;
  numberTarget = target;
  bool isWeight = target == sbWeight;
  bool isSleep = target == sbSleep;
  numberMax = isWeight ? 10000 : isSleep ? 60 : 999;

  numberOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(numberOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(numberOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(numberOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(numberOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(numberOverlay, 0, 0);
  lv_obj_set_style_radius(numberOverlay, 0, 0);
  lv_obj_set_style_pad_all(numberOverlay, 0, 0);
  lv_obj_clear_flag(numberOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(numberOverlay);
  lv_label_set_text(title, isWeight ? T(StrId::Weight) : isSleep ? T(StrId::SleepTimeout) : T(StrId::SpoolNumber));
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 8);

  numberInput = lv_textarea_create(numberOverlay);
  lv_textarea_set_one_line(numberInput, true);
  lv_textarea_set_accepted_chars(numberInput, "0123456789");
  lv_textarea_set_max_length(numberInput, isWeight ? 5 : isSleep ? 2 : 3);
  lv_textarea_set_placeholder_text(numberInput, String(getNumberField(target)).c_str());
  lv_obj_set_size(numberInput, SCREEN_WIDTH - 30, 46);
  lv_obj_align(numberInput, LV_ALIGN_TOP_LEFT, 15, 30);

  lv_obj_t *keyboardWidget = lv_keyboard_create(numberOverlay);
  lv_keyboard_set_mode(keyboardWidget, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_set_size(keyboardWidget, SCREEN_WIDTH, 230);
  lv_obj_align(keyboardWidget, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboardWidget, numberInput);
  lv_obj_add_event_cb(keyboardWidget, numberKeyboardCb, LV_EVENT_ALL, nullptr);
}

static lv_obj_t *makeNumberField(lv_obj_t *parent, int x, int y, int width) {
  lv_obj_t *field = lv_textarea_create(parent);
  lv_textarea_set_one_line(field, true);
  lv_textarea_set_text(field, "0");
  lv_obj_set_size(field, width, 40);
  lv_obj_align(field, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_add_event_cb(field, openNumberOverlay, LV_EVENT_CLICKED, field);
  return field;
}

static void setNewSpoolFieldsEnabled(bool enabled) {
  auto set = [enabled](lv_obj_t *obj) {
    if (enabled) {
      lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
  };
  set(ddMaterial);
  set(btnColor);
  set(ddSpoolmanVendor);
  set(btnFilament);
}

static void spoolDropdownCb(lv_event_t *e);
static void filamentDropdownCb(lv_event_t *e);

static bool parseColorHex(const String &hex, uint32_t &rgb) {
  String h = hex;
  if (h.startsWith("#")) h.remove(0, 1);
  if (h.length() < 6) return false;
  for (int i = 0; i < 6; i++) {
    if (!isxdigit((unsigned char)h[i])) return false;
  }
  rgb = (uint32_t)strtoul(h.substring(0, 6).c_str(), nullptr, 16);
  return true;
}

// Text and colour of entry `index` of the Spool (forSpool) or Filament
// picker; index 0 is "New spool" / "(pick to prefill)" and has no colour.
static void pickerEntry(bool forSpool, size_t index, String &text, bool &hasColor, uint32_t &rgb) {
  hasColor = false;
  if (index == 0) {
    text = forSpool ? T(StrId::NewSpool) : T(StrId::NoFilament);
    return;
  }
  if (forSpool) {
    if (index - 1 >= uiSpools.size()) return;
    const SpoolmanSpool &spool = uiSpools[index - 1];
    text = "#" + String(spool.id) + " " + spool.material;
    if (spool.vendorName.length()) text += " (" + spool.vendorName + ")";
    text += " - " + String(spool.remainingWeightGrams) + "g";
    hasColor = parseColorHex(spool.colorHex, rgb);
  } else {
    if (index - 1 >= uiFilaments.size()) return;
    const SpoolmanFilament &filament = uiFilaments[index - 1];
    text = filament.vendorName + " " + filament.material;
    hasColor = parseColorHex(filament.colorHex, rgb);
  }
}

static void paintSwatch(lv_obj_t *swatch, bool hasColor, uint32_t rgb) {
  lv_obj_set_style_bg_color(swatch, lv_color_hex(hasColor ? rgb : 0x707070), 0);
  lv_obj_set_style_bg_opa(swatch, hasColor ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void updateSpoolButton() {
  String text;
  bool hasColor;
  uint32_t rgb = 0;
  pickerEntry(true, lv_dropdown_get_selected(ddSpool), text, hasColor, rgb);
  lv_label_set_text(spoolBtnLabel, text.c_str());
  paintSwatch(spoolSwatch, hasColor, rgb);
}

static void updateFilamentButton() {
  String text;
  bool hasColor;
  uint32_t rgb = 0;
  pickerEntry(false, lv_dropdown_get_selected(ddFilament), text, hasColor, rgb);
  lv_label_set_text(filamentBtnLabel, text.c_str());
  paintSwatch(filamentSwatch, hasColor, rgb);
}

static void closePicker() {
  if (pickerOverlay) {
    lv_obj_del_async(pickerOverlay);
    pickerOverlay = nullptr;
  }
}

static void pickerItemCb(lv_event_t *e) {
  uint16_t index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  if (pickerForSpool) {
    lv_dropdown_set_selected(ddSpool, index);
    spoolDropdownCb(nullptr);
  } else {
    lv_dropdown_set_selected(ddFilament, index);
    filamentDropdownCb(nullptr);
  }
  closePicker();
}

// One row/button: colour swatch on the left, text on the right.
static lv_obj_t *makeSwatchButton(lv_obj_t *parent, int width, int height, int textWidth,
                                  lv_obj_t **swatchOut, lv_obj_t **labelOut) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);

  lv_obj_t *swatch = lv_obj_create(btn);
  lv_obj_set_size(swatch, 24, 24);
  lv_obj_set_style_radius(swatch, 4, 0);
  lv_obj_set_style_border_width(swatch, 1, 0);
  lv_obj_set_style_border_color(swatch, lv_color_hex(0x909090), 0);
  lv_obj_set_style_pad_all(swatch, 0, 0);
  lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(swatch, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_width(label, textWidth);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 32, 0);

  *swatchOut = swatch;
  *labelOut = label;
  return btn;
}

// Full-screen scrollable list where every entry shows its colour.
static void openPicker(bool forSpool) {
  if (pickerOverlay) return;
  pickerForSpool = forSpool;

  pickerOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(pickerOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(pickerOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(pickerOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(pickerOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(pickerOverlay, 0, 0);
  lv_obj_set_style_radius(pickerOverlay, 0, 0);
  lv_obj_set_style_pad_all(pickerOverlay, 0, 0);
  lv_obj_clear_flag(pickerOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      pickerOverlay,
      [](lv_event_t *ev) {
        if (lv_event_get_target(ev) == lv_event_get_current_target(ev)) closePicker();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *list = lv_obj_create(pickerOverlay);
  lv_obj_set_size(list, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_pad_row(list, 4, 0);

  size_t count = 1 + (forSpool ? uiSpools.size() : uiFilaments.size());
  for (size_t i = 0; i < count; i++) {
    String text;
    bool hasColor;
    uint32_t rgb = 0;
    pickerEntry(forSpool, i, text, hasColor, rgb);

    lv_obj_t *swatch, *label;
    lv_obj_t *row = makeSwatchButton(list, SCREEN_WIDTH - 44, 40, SCREEN_WIDTH - 100, &swatch, &label);
    lv_label_set_text(label, text.c_str());
    paintSwatch(swatch, hasColor, rgb);
    lv_obj_add_event_cb(row, pickerItemCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }
}

// A chosen filament fixes material, colour and vendor of the new spool, so
// only the initial weight stays editable; "(pick to prefill)" frees them again.
static void setFilamentFieldsEnabled(bool enabled) {
  lv_obj_t *fields[] = {ddMaterial, btnColor, ddSpoolmanVendor};
  for (lv_obj_t *field : fields) {
    if (enabled) {
      lv_obj_clear_state(field, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(field, LV_STATE_DISABLED);
    }
  }
}

static void filamentDropdownCb(lv_event_t *e) {
  updateFilamentButton();
  uint16_t index = lv_dropdown_get_selected(ddFilament);
  if (index == 0 || index - 1 >= uiFilaments.size()) {
    setFilamentFieldsEnabled(true); // "(pick to prefill)"
    return;
  }
  setFilamentFieldsEnabled(false);

  const SpoolmanFilament &filament = uiFilaments[index - 1];
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(filament.material.c_str()));
  selectColorCode(closestQidiColorCode(filament.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == filament.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
}

static void spoolDropdownCb(lv_event_t *e) {
  updateSpoolButton();
  uint16_t index = lv_dropdown_get_selected(ddSpool);
  if (index == 0 || index - 1 >= uiSpools.size()) {
    setNewSpoolFieldsEnabled(true);
    setNumberField(sbSpoolNumber, 0);
    lv_dropdown_set_selected(ddFilament, 0);
    updateFilamentButton();
    return;
  }
  const SpoolmanSpool &spool = uiSpools[index - 1];
  setNumberField(sbSpoolNumber, spool.id);
  lv_dropdown_set_selected(ddFilament, 0);
  for (size_t i = 0; i < uiFilaments.size(); i++) {
    if (uiFilaments[i].id == spool.filamentId) {
      lv_dropdown_set_selected(ddFilament, i + 1);
      break;
    }
  }
  updateFilamentButton();
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(spool.material.c_str()));
  selectColorCode(closestQidiColorCode(spool.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == spool.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
  setNumberField(sbWeight, spool.remainingWeightGrams);
  setNewSpoolFieldsEnabled(false);
}

static void refreshSettingsScreen() {
  setNumberField(sbSleep, settings.sleepMinutes);
  lv_textarea_set_text(taSpoolmanAddress, settings.spoolmanAddress.c_str());
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

static void useSpoolmanToggleCb(lv_event_t *e) {
  settings.useSpoolman = lv_obj_has_state(swUseSpoolman, LV_STATE_CHECKED);
  settings.save();
}

static void saveSpoolmanAddress() {
  String address = lv_textarea_get_text(taSpoolmanAddress);
  address.trim();
  while (address.endsWith("/")) address.remove(address.length() - 1);
  if (address.length() && !address.startsWith("http://") && !address.startsWith("https://")) {
    address = "http://" + address;
  }
  lv_textarea_set_text(taSpoolmanAddress, address.c_str());
  settings.spoolmanAddress = address;
  settings.useSpoolman = address.length() > 0;
  if (settings.useSpoolman) {
    lv_obj_add_state(swUseSpoolman, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(swUseSpoolman, LV_STATE_CHECKED);
  }
  settings.save();
}

static void spoolmanAddressFocusCb(lv_event_t *e) {
  lv_keyboard_set_textarea(keyboard, taSpoolmanAddress);
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void spoolmanAddressDefocusCb(lv_event_t *e) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  saveSpoolmanAddress();
}

static void keyboardReadyCb(lv_event_t *e) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  saveSpoolmanAddress();
}

static void reconfigureNetworkCb(lv_event_t *e) { wifiSetupReset(); }

static lv_obj_t *labeledNumberField(lv_obj_t *parent, const char *labelText, int x, int y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, labelText);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
  return makeNumberField(parent, x, y + 17, 140);
}

static void buildMainScreen() {
  scrMain = lv_obj_create(nullptr);

  lv_obj_t *title = lv_label_create(scrMain);
  lv_label_set_text(title, T(StrId::AppTitle));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  lv_obj_t *version = lv_label_create(scrMain);
  lv_label_set_text(version, "v" FIRMWARE_VERSION);
  lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(version, lv_color_hex(0x808080), 0);
  lv_obj_align_to(version, title, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -2);

  lv_obj_t *btnSettings = lv_btn_create(scrMain);
  lv_obj_set_size(btnSettings, 50, 40);
  lv_obj_align(btnSettings, LV_ALIGN_TOP_RIGHT, -10, 8);
  lv_obj_add_event_cb(btnSettings, settingsButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblGear = lv_label_create(btnSettings);
  lv_label_set_text(lblGear, LV_SYMBOL_SETTINGS);
  lv_obj_center(lblGear);

  lblStatus = lv_label_create(scrMain);
  lv_label_set_text(lblStatus, T(StrId::Ready));
  lv_obj_set_width(lblStatus, SCREEN_WIDTH - 40);
  lv_label_set_long_mode(lblStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblStatus, LV_ALIGN_TOP_MID, 0, 60);

  struct Entry { const char *text; lv_event_cb_t cb; int x; };
  Entry entries[2] = {
      {T(StrId::ReadTag), readButtonCb, -110},
      {T(StrId::WriteTag), writeButtonCb, 110},
  };
  for (const Entry &entry : entries) {
    lv_obj_t *btn = lv_btn_create(scrMain);
    lv_obj_set_size(btn, 190, 90);
    lv_obj_align(btn, LV_ALIGN_CENTER, entry.x, 45);
    lv_obj_add_event_cb(btn, entry.cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, entry.text);
    lv_obj_center(label);
  }
}

static void buildEditScreen() {
  scrEdit = lv_obj_create(nullptr);

  // Two columns, four rows; every field has its label above it.
  //   1: Spool | Filament            (Spoolman mode only)
  //   2: Material | Colour
  //   3: Spoolman vendor (Vendor in plain mode) | Spool number
  //   4: Weight (+ status line)
  const int leftX = 15, rightX = 250, colW = 215;
  const int rowY[4] = {2, 58, 114, 170};
  const int controlDy = 17;

  auto makeLabel = [](int x, int y, const char *text) {
    lv_obj_t *label = lv_label_create(scrEdit);
    if (text) lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
  };

  // Row 1
  lblSpoolRow = makeLabel(leftX, rowY[0], T(StrId::Spool));
  ddSpool = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpool, colW);
  lv_obj_align(ddSpool, LV_ALIGN_TOP_LEFT, leftX, rowY[0] + controlDy);
  lv_dropdown_set_options(ddSpool, T(StrId::NewSpool));
  lv_obj_add_event_cb(ddSpool, spoolDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddSpool, LV_OBJ_FLAG_HIDDEN); // holds the selection; never shown
  btnSpool = makeSwatchButton(scrEdit, colW, 38, colW - 44, &spoolSwatch, &spoolBtnLabel);
  lv_obj_align(btnSpool, LV_ALIGN_TOP_LEFT, leftX, rowY[0] + controlDy);
  lv_obj_add_event_cb(btnSpool, [](lv_event_t *e) { openPicker(true); }, LV_EVENT_CLICKED, nullptr);
  updateSpoolButton();

  lblFilament = makeLabel(rightX, rowY[0], T(StrId::Filament));
  ddFilament = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddFilament, colW);
  lv_obj_align(ddFilament, LV_ALIGN_TOP_LEFT, rightX, rowY[0] + controlDy);
  lv_dropdown_set_options(ddFilament, T(StrId::NoFilament));
  lv_obj_add_event_cb(ddFilament, filamentDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddFilament, LV_OBJ_FLAG_HIDDEN); // holds the selection; never shown
  btnFilament = makeSwatchButton(scrEdit, colW, 38, colW - 44, &filamentSwatch, &filamentBtnLabel);
  lv_obj_align(btnFilament, LV_ALIGN_TOP_LEFT, rightX, rowY[0] + controlDy);
  lv_obj_add_event_cb(btnFilament, [](lv_event_t *e) { openPicker(false); }, LV_EVENT_CLICKED, nullptr);
  updateFilamentButton();

  // Row 2
  makeLabel(leftX, rowY[1], T(StrId::Material));
  ddMaterial = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddMaterial, colW);
  lv_obj_align(ddMaterial, LV_ALIGN_TOP_LEFT, leftX, rowY[1] + controlDy);
  buildDropdownOptions(ddMaterial, qidiMaterials, qidiMaterialsCount);

  makeLabel(rightX, rowY[1], T(StrId::Color));
  ddColor = lv_dropdown_create(scrEdit); // holds the selection; never shown
  buildDropdownOptions(ddColor, qidiColors, qidiColorsCount);
  lv_obj_add_flag(ddColor, LV_OBJ_FLAG_HIDDEN);
  btnColor = lv_btn_create(scrEdit);
  lv_obj_set_size(btnColor, colW, 38);
  lv_obj_align(btnColor, LV_ALIGN_TOP_LEFT, rightX, rowY[1] + controlDy);
  lv_obj_add_event_cb(btnColor, openColorOverlay, LV_EVENT_CLICKED, nullptr);
  lblColorHex = lv_label_create(btnColor);
  lv_obj_center(lblColorHex);
  updateColorButton();

  // Row 3, left: Spoolman vendor (Spoolman mode) or QIDI vendor (plain mode)
  lblRow3 = makeLabel(leftX, rowY[2], nullptr);
  ddVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddVendor, colW);
  lv_obj_align(ddVendor, LV_ALIGN_TOP_LEFT, leftX, rowY[2] + controlDy);
  buildDropdownOptions(ddVendor, qidiVendors, qidiVendorsCount);
  ddSpoolmanVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpoolmanVendor, colW);
  lv_obj_align(ddSpoolmanVendor, LV_ALIGN_TOP_LEFT, leftX, rowY[2] + controlDy);

  // Row 3, right: spool number (typed in plain mode, shown read-only in
  // Spoolman mode where it comes from the selected/created spool)
  makeLabel(rightX, rowY[2], (String(T(StrId::SpoolNumber)) + " (0-999)").c_str());
  sbSpoolNumber = makeNumberField(scrEdit, rightX, rowY[2] + controlDy, 140);

  // Row 4: weight, with the status line to its right
  sbWeight = labeledNumberField(scrEdit, T(StrId::Weight), leftX, rowY[3]);

  lblEditStatus = lv_label_create(scrEdit);
  lv_obj_set_width(lblEditStatus, 295);
  lv_label_set_long_mode(lblEditStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblEditStatus, LV_ALIGN_TOP_LEFT, 170, rowY[3] + controlDy + 2);

  lv_obj_t *btnSave = lv_btn_create(scrEdit);
  lv_obj_set_size(btnSave, colW, 46);
  lv_obj_align(btnSave, LV_ALIGN_BOTTOM_LEFT, leftX, -8);
  lv_obj_add_event_cb(btnSave, saveButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblSave = lv_label_create(btnSave);
  lv_label_set_text(lblSave, T(StrId::WriteTag));
  lv_obj_center(lblSave);

  lv_obj_t *btnBack = lv_btn_create(scrEdit);
  lv_obj_set_size(btnBack, colW, 46);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_RIGHT, -15, -8);
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
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  lblWifiStatus = lv_label_create(scrSettings);
  lv_obj_set_width(lblWifiStatus, SCREEN_WIDTH - 30);
  lv_label_set_long_mode(lblWifiStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblWifiStatus, LV_ALIGN_TOP_LEFT, 15, 36);

  taSpoolmanAddress = lv_textarea_create(scrSettings);
  lv_textarea_set_one_line(taSpoolmanAddress, true);
  lv_textarea_set_placeholder_text(taSpoolmanAddress, T(StrId::SpoolmanAddress));
  lv_obj_set_size(taSpoolmanAddress, SCREEN_WIDTH - 30, 42);
  lv_obj_align(taSpoolmanAddress, LV_ALIGN_TOP_LEFT, 15, 66);
  lv_obj_add_event_cb(taSpoolmanAddress, spoolmanAddressFocusCb, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(taSpoolmanAddress, spoolmanAddressDefocusCb, LV_EVENT_DEFOCUSED, nullptr);

  lv_obj_t *lblUseSpoolman = lv_label_create(scrSettings);
  lv_label_set_text(lblUseSpoolman, T(StrId::UseSpoolman));
  lv_obj_align(lblUseSpoolman, LV_ALIGN_TOP_LEFT, 15, 124);
  swUseSpoolman = lv_switch_create(scrSettings);
  lv_obj_align(swUseSpoolman, LV_ALIGN_TOP_LEFT, 200, 118);
  lv_obj_add_event_cb(swUseSpoolman, useSpoolmanToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *lblSleep = lv_label_create(scrSettings);
  lv_label_set_text(lblSleep, T(StrId::SleepTimeout));
  lv_obj_align(lblSleep, LV_ALIGN_TOP_LEFT, 15, 176);
  sbSleep = makeNumberField(scrSettings, 340, 166, 120);
  setNumberField(sbSleep, settings.sleepMinutes);

  lv_obj_t *btnReconfigure = lv_btn_create(scrSettings);
  lv_obj_set_size(btnReconfigure, 265, 50);
  lv_obj_align(btnReconfigure, LV_ALIGN_BOTTOM_LEFT, 15, -10);
  lv_obj_add_event_cb(btnReconfigure, reconfigureNetworkCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblReconfigure = lv_label_create(btnReconfigure);
  lv_label_set_text(lblReconfigure, T(StrId::ReconfigureNetwork));
  lv_obj_center(lblReconfigure);

  lv_obj_t *btnBack = lv_btn_create(scrSettings);
  lv_obj_set_size(btnBack, 170, 50);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
  lv_obj_add_event_cb(btnBack, backButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, T(StrId::Back));
  lv_obj_center(lblBack);

  keyboard = lv_keyboard_create(scrSettings);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, keyboardReadyCb, LV_EVENT_READY, nullptr);
}

void uiInit() {
  buildMainScreen();
  buildEditScreen();
  buildSettingsScreen();
  lv_label_set_text(lblWifiStatus, T(StrId::WifiStatus));
  lv_scr_load(scrMain);
}

void uiSetStatus(const char *text) {
  lv_label_set_text(lblStatus, text);
  if (lblEditStatus) lv_label_set_text(lblEditStatus, text);
}

void uiSetWifiStatus(const char *text) {
  lv_label_set_text(lblWifiStatus, text);
}

void uiShowSpool(const FilamentSpool &spool) {
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount, spool.materialCode);
  selectColorCode(spool.colorCode);
  selectCode(ddVendor, qidiVendors, qidiVendorsCount,
             spool.internalVendorId != 0 ? qidiVendorCode("GENERIC") : spool.vendorCode);
  setNumberField(sbSpoolNumber, spool.spoolNumber);
  setNumberField(sbWeight, spool.lastWeightGrams);

  if (spoolmanModeActive) {
    // Point the Spool selector at the matching cached spool, if any, so
    // writing back does not create a duplicate; otherwise leave "New spool"
    // selected with the tag's own fields as a manual fallback.
    lv_dropdown_set_selected(ddSpool, 0);
    updateSpoolButton();
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
    setNumberField(sbSpoolNumber, 0);
    lv_dropdown_set_selected(ddSpool, 0);
    updateSpoolButton();
    setNewSpoolFieldsEnabled(true);
  }
  lv_scr_load(scrEdit);
}

void uiSetSpoolmanMode(bool active) {
  spoolmanModeActive = active;
  lv_label_set_text(lblRow3, active ? T(StrId::SpoolmanVendor) : T(StrId::Vendor));

  auto show = [](lv_obj_t *obj, bool visible) {
    if (visible) {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
  };
  show(ddVendor, !active);
  show(ddSpoolmanVendor, active);
  show(lblSpoolRow, active);
  show(btnSpool, active);
  show(lblFilament, active);
  show(btnFilament, active);

  // In Spoolman mode the number comes from the selected/created spool.
  if (active) {
    lv_obj_add_state(sbSpoolNumber, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(sbSpoolNumber, LV_STATE_DISABLED);
  }
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
    spoolOptions += " - " + String(spool.remainingWeightGrams) + "g";
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
  updateSpoolButton();
  updateFilamentButton();
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

int uiSelectedFilamentId() {
  if (!spoolmanModeActive || !uiIsNewSpoolSelected()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddFilament);
  if (index == 0 || index - 1 >= uiFilaments.size()) return -1;
  return uiFilaments[index - 1].id;
}

int uiSelectedSpoolmanVendorId() {
  if (!spoolmanModeActive) return -1;
  if (!uiIsNewSpoolSelected()) {
    // An existing spool brings its own vendor, whatever the dropdown shows.
    uint16_t spoolIndex = lv_dropdown_get_selected(ddSpool);
    if (spoolIndex - 1 < uiSpools.size() && uiSpools[spoolIndex - 1].vendorId > 0) {
      return uiSpools[spoolIndex - 1].vendorId;
    }
    return -1;
  }
  if (uiVendors.empty()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddSpoolmanVendor);
  if (index >= uiVendors.size()) return -1;
  return uiVendors[index].id;
}

FilamentSpool uiCurrentSpool() {
  FilamentSpool spool;
  spool.materialCode = selectedCode(ddMaterial, qidiMaterials, qidiMaterialsCount);
  spool.colorCode = selectedCode(ddColor, qidiColors, qidiColorsCount);
  spool.lastWeightGrams = (uint16_t)getNumberField(sbWeight);

  if (spoolmanModeActive) {
    // spoolNumber/internalVendorId/vendorCode are resolved by main.cpp
    // (existing spool id, or a newly created one) after this call.
    // Without a linked Spoolman vendor the tag carries the QIDI vendor code.
    spool.vendorCode = qidiVendorCode("QIDI");
    spool.internalVendorId = 0;
    spool.spoolNumber = 0;
  } else {
    spool.vendorCode = selectedCode(ddVendor, qidiVendors, qidiVendorsCount);
    spool.internalVendorId = 0;
    spool.spoolNumber = (uint16_t)getNumberField(sbSpoolNumber);
  }
  return spool;
}
