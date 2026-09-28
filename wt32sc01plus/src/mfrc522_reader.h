#pragma once
#include <Arduino.h>
#include "qidi_tag.h"

enum class TagResult {
  Ok,
  NoTag,
  NotMifareClassic1k,
  AuthenticationFailed,
  ReadFailed,
  WriteFailed,
};

class Mfrc522Reader {
public:
  bool begin();

  // Waits up to timeoutMs for a tag; true if one answered.
  bool waitForTag(uint32_t timeoutMs = 3000);

  // Reads the 16-byte spool payload from qidiTagBlock. The tag must already
  // have answered waitForTag().
  TagResult readSpoolBytes(uint8_t out[qidiTagLength]);

  // Writes and reads back the 16-byte spool payload at qidiTagBlock.
  TagResult writeSpoolBytes(const uint8_t data[qidiTagLength]);

private:
  bool authenticateSector1();

  bool _tagPresent = false;
};
