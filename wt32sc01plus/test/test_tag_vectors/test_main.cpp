// Runs test/tag_vectors.json - the same file RFIDwisp's
// test/tag_vectors_test.dart runs against the Dart implementation - against
// the C++ port in src/qidi_tag.*. Plain main(), no test framework: run with
// `pio test -e native` (test_framework = custom) from this directory.
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "../../src/qidi_tag.h"

static int failures = 0;
static int checks = 0;

static void fail(const char *kind, const char *name, const char *what) {
  failures++;
  printf("FAIL %s: %s - %s\n", kind, name, what);
}

static bool hexToBytes(const char *hex, uint8_t out[qidiTagLength]) {
  if (strlen(hex) != qidiTagLength * 2) return false;
  for (size_t i = 0; i < qidiTagLength; i++) {
    char pair[3] = {hex[i * 2], hex[i * 2 + 1], 0};
    out[i] = (uint8_t)strtol(pair, nullptr, 16);
  }
  return true;
}

static std::string toHex(const uint8_t *bytes) {
  std::string s;
  char buf[3];
  for (size_t i = 0; i < qidiTagLength; i++) {
    snprintf(buf, sizeof(buf), "%02x", bytes[i]);
    s += buf;
  }
  return s;
}

static bool spoolFromJson(JsonObjectConst json, FilamentSpool &spool) {
  int material = qidiMaterialCode(json["material"] | "");
  int color = qidiColorCode(json["colorHex"] | "");
  int vendor = qidiVendorCode(json["vendor"] | "");
  if (material < 0 || color < 0 || vendor < 0) return false;
  spool.materialCode = (uint8_t)material;
  spool.colorCode = (uint8_t)color;
  spool.vendorCode = (uint8_t)vendor;
  spool.spoolNumber = (uint16_t)(json["spoolNumber"] | 0);
  spool.internalVendorId = (uint8_t)(json["internalVendorId"] | 0);
  spool.lastWeightGrams = (uint16_t)(json["lastWeightGrams"] | 0);
  return true;
}

static bool sameSpool(const FilamentSpool &a, const FilamentSpool &b) {
  return a.materialCode == b.materialCode && a.colorCode == b.colorCode &&
         a.vendorCode == b.vendorCode && a.spoolNumber == b.spoolNumber &&
         a.internalVendorId == b.internalVendorId && a.lastWeightGrams == b.lastWeightGrams;
}

static void checkRead(const char *name, const uint8_t bytes[qidiTagLength], JsonObjectConst expected,
                      bool expectValid) {
  checks++;
  FilamentSpool read;
  bool valid = FilamentSpool::fromTagBytes(bytes, read);
  if (valid != expectValid) {
    fail("reads", name, expectValid ? "tag was rejected" : "tag was accepted");
    return;
  }
  if (!valid) return;
  FilamentSpool want;
  if (!spoolFromJson(expected, want)) {
    fail("reads", name, "vector uses a name this firmware does not know");
    return;
  }
  if (!sameSpool(read, want)) fail("reads", name, "decoded a different spool");
}

int main() {
  const char *path = getenv("TAG_VECTORS");
  std::ifstream file(path ? path : "test/tag_vectors.json", std::ios::binary);
  if (!file) {
    printf("cannot open test/tag_vectors.json (run from wt32sc01plus/ or set TAG_VECTORS)\n");
    return 2;
  }
  std::stringstream text;
  text << file.rdbuf();
  JsonDocument doc;
  if (deserializeJson(doc, text.str())) {
    printf("tag_vectors.json is not valid JSON\n");
    return 2;
  }

  for (JsonObjectConst c : doc["roundtrip"].as<JsonArrayConst>()) {
    const char *name = c["name"];
    uint8_t want[qidiTagLength];
    FilamentSpool spool;
    if (!hexToBytes(c["bytes"] | "", want) || !spoolFromJson(c["spool"], spool)) {
      fail("vector", name, "malformed vector");
      continue;
    }
    checks++;
    uint8_t got[qidiTagLength];
    spool.toTagBytes(got);
    if (memcmp(got, want, qidiTagLength) != 0) {
      fail("writes", name, (toHex(got) + " != " + toHex(want)).c_str());
    }
    checkRead(name, want, c["spool"], true);
  }

  for (JsonObjectConst c : doc["fromTagOnly"].as<JsonArrayConst>()) {
    const char *name = c["name"];
    uint8_t bytes[qidiTagLength];
    if (!hexToBytes(c["bytes"] | "", bytes)) {
      fail("vector", name, "malformed vector");
      continue;
    }
    checkRead(name, bytes, c["spool"], !c["spool"].isNull());
  }

  for (JsonObjectConst c : doc["blank"].as<JsonArrayConst>()) {
    const char *name = c["name"];
    uint8_t bytes[qidiTagLength];
    if (!hexToBytes(c["bytes"] | "", bytes)) {
      fail("vector", name, "malformed vector");
      continue;
    }
    checks++;
    if (FilamentSpool::isBlankTagBytes(bytes) != c["blank"].as<bool>()) {
      fail("blank", name, "wrong blank detection");
    }
  }

  printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
