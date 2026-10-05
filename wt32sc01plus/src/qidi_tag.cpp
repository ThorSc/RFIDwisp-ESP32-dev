#include "qidi_tag.h"
#include <string.h>
#include <stdlib.h>

static char toLowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static bool equalsIgnoreCase(const char *a, const char *b) {
  while (*a && *b) {
    if (toLowerAscii(*a) != toLowerAscii(*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

static bool startsWithIgnoreCase(const char *haystack, const char *needle) {
  while (*needle) {
    if (*haystack == '\0' || toLowerAscii(*haystack) != toLowerAscii(*needle)) return false;
    haystack++;
    needle++;
  }
  return true;
}

const NamedByte qidiMaterials[] = {
    {"PLA Rapido", 0x01},
    {"PLA Matte", 0x02},
    {"PLA Metal", 0x03},
    {"PLA Silk", 0x04},
    {"PLA-CF", 0x05},
    {"PLA-Wood", 0x06},
    {"PLA Basic", 0x07},
    {"PLA Matte Basic", 0x08},
    {"Support For PLA", 0x0A},
    {"ABS Rapido", 0x0B},
    {"ABS-GF", 0x0C},
    {"ABS-Metal", 0x0D},
    {"ABS-Odorless", 0x0E},
    {"TPU-GF", 0x0F},
    {"ASA", 0x12},
    {"ASA-Aero", 0x13},
    {"ASA-CF", 0x14},
    {"PC", 0x17},
    {"UltraPA", 0x18},
    {"PA-CF", 0x19},
    {"UltraPA-CF25", 0x1A},
    {"PA12-CF", 0x1B},
    {"PAHT-CF", 0x1E},
    {"PAHT-GF", 0x1F},
    {"Support For PAHT", 0x20},
    {"Support For PET/PA", 0x21},
    {"PC/ABS-FR", 0x22},
    {"TPEE", 0x23},
    {"PEBA", 0x24},
    {"PET-CF", 0x25},
    {"PET-GF", 0x26},
    {"PETG Basic", 0x27},
    {"PETG-Tough", 0x28},
    {"PETG Rapido", 0x29},
    {"PETG-CF", 0x2A},
    {"PETG-GF", 0x2B},
    {"PPS-CF", 0x2C},
    {"PETG Translucent", 0x2D},
    {"PPS-GF", 0x2E},
    {"PVA", 0x2F},
    {"TPU-AERO 64D", 0x30},
    {"TPU-Aero", 0x31},
    {"TPU 95A-HF", 0x32},
};
const size_t qidiMaterialsCount = sizeof(qidiMaterials) / sizeof(qidiMaterials[0]);

const NamedByte qidiColors[] = {
    {"#FAFAFA", 0x01}, {"#060606", 0x02}, {"#D9E3ED", 0x03}, {"#5CF30F", 0x04},
    {"#63E492", 0x05}, {"#2850FF", 0x06}, {"#FE98FE", 0x07}, {"#DFD628", 0x08},
    {"#228332", 0x09}, {"#99DEFF", 0x0A}, {"#1714B0", 0x0B}, {"#CEC0FE", 0x0C},
    {"#CADE4B", 0x0D}, {"#1353AB", 0x0E}, {"#5EA9FD", 0x0F}, {"#A878FF", 0x10},
    {"#FE717A", 0x11}, {"#FF362D", 0x12}, {"#E2DFCD", 0x13}, {"#898F9B", 0x14},
    {"#6E3812", 0x15}, {"#CAC59F", 0x16}, {"#F28636", 0x17}, {"#B87F2B", 0x18},
};
const size_t qidiColorsCount = sizeof(qidiColors) / sizeof(qidiColors[0]);

const NamedByte qidiVendors[] = {
    {"GENERIC", 0},
    {"QIDI", 1},
};
const size_t qidiVendorsCount = sizeof(qidiVendors) / sizeof(qidiVendors[0]);

const char *qidiMaterialName(uint8_t code) {
  for (size_t i = 0; i < qidiMaterialsCount; i++) {
    if (qidiMaterials[i].code == code) return qidiMaterials[i].name;
  }
  return nullptr;
}

const char *qidiColorHex(uint8_t code) {
  for (size_t i = 0; i < qidiColorsCount; i++) {
    if (qidiColors[i].code == code) return qidiColors[i].name;
  }
  return nullptr;
}

const char *qidiVendorName(uint8_t code) {
  for (size_t i = 0; i < qidiVendorsCount; i++) {
    if (qidiVendors[i].code == code) return qidiVendors[i].name;
  }
  return nullptr;
}

int qidiMaterialCode(const char *name) {
  for (size_t i = 0; i < qidiMaterialsCount; i++) {
    if (strcmp(qidiMaterials[i].name, name) == 0) return qidiMaterials[i].code;
  }
  return -1;
}

int qidiColorCode(const char *hex) {
  for (size_t i = 0; i < qidiColorsCount; i++) {
    if (strcmp(qidiColors[i].name, hex) == 0) return qidiColors[i].code;
  }
  return -1;
}

int qidiVendorCode(const char *name) {
  for (size_t i = 0; i < qidiVendorsCount; i++) {
    if (strcmp(qidiVendors[i].name, name) == 0) return qidiVendors[i].code;
  }
  return -1;
}

static bool parseHex6(const char *hex, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (hex[0] == '#') hex++;
  // Spoolman sometimes sends RRGGBBAA; only the first 6 digits are the colour.
  if (strlen(hex) < 6) return false;
  for (int i = 0; i < 6; i++) {
    char c = hex[i];
    bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!isHex) return false;
  }
  char buf[3] = {0, 0, 0};
  buf[0] = hex[0]; buf[1] = hex[1];
  r = (uint8_t)strtol(buf, nullptr, 16);
  buf[0] = hex[2]; buf[1] = hex[3];
  g = (uint8_t)strtol(buf, nullptr, 16);
  buf[0] = hex[4]; buf[1] = hex[5];
  b = (uint8_t)strtol(buf, nullptr, 16);
  return true;
}

uint8_t closestQidiColorCode(const char *hex) {
  uint8_t tr, tg, tb;
  if (!parseHex6(hex, tr, tg, tb)) return qidiColors[0].code;

  uint8_t best = qidiColors[0].code;
  long bestDistance = -1;
  for (size_t i = 0; i < qidiColorsCount; i++) {
    uint8_t cr, cg, cb;
    if (!parseHex6(qidiColors[i].name, cr, cg, cb)) continue;
    long dr = (long)cr - tr, dg = (long)cg - tg, db = (long)cb - tb;
    long distance = dr * dr + dg * dg + db * db;
    if (bestDistance < 0 || distance < bestDistance) {
      bestDistance = distance;
      best = qidiColors[i].code;
    }
  }
  return best;
}

static bool containsCaseInsensitive(const char *haystack, const char *needle) {
  if (*needle == '\0') return false;
  for (size_t i = 0; haystack[i] != '\0'; i++) {
    if (startsWithIgnoreCase(haystack + i, needle)) return true;
  }
  return false;
}

uint8_t closestQidiMaterialCode(const char *material) {
  for (size_t i = 0; i < qidiMaterialsCount; i++) {
    if (equalsIgnoreCase(qidiMaterials[i].name, material)) return qidiMaterials[i].code;
  }
  for (size_t i = 0; i < qidiMaterialsCount; i++) {
    if (containsCaseInsensitive(qidiMaterials[i].name, material)) return qidiMaterials[i].code;
  }
  return qidiMaterials[0].code;
}
