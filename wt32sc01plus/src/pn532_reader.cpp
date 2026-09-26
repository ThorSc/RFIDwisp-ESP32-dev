#include "pn532_reader.h"
#include "board_config.h"
#include <Wire.h>
#include <Adafruit_PN532.h>

// Second I2C bus (Wire1), separate from the on-board touch controller's bus.
static TwoWire PN532Wire(1);
static Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &PN532Wire);

// Same default keys, in the same try order, as RFIDwisp's mifare.dart:
// each key is tried as key A, then as key B, until one authenticates.
static const uint8_t defaultKeys[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // factory default
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // common default
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, // MAD key
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, // common key
};
static const size_t defaultKeysCount = sizeof(defaultKeys) / sizeof(defaultKeys[0]);

bool Pn532Reader::begin() {
  PN532Wire.begin(PN532_SDA, PN532_SCL);
  nfc.begin();

  uint32_t version = nfc.getFirmwareVersion();
  if (!version) return false; // PN532 not found - check wiring/pins

  nfc.SAMConfig();
  return true;
}

bool Pn532Reader::waitForTag(uint32_t timeoutMs) {
  _uidLength = 0;
  return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, _uid, &_uidLength,
                                  timeoutMs);
}

bool Pn532Reader::authenticateSector1() {
  if (_uidLength == 0) return false;
  for (size_t i = 0; i < defaultKeysCount; i++) {
    for (uint8_t keyType = 0; keyType <= 1; keyType++) { // 0 = key A, 1 = key B
      if (nfc.mifareclassic_AuthenticateBlock(_uid, _uidLength, qidiTagBlock,
                                               keyType,
                                               (uint8_t *)defaultKeys[i])) {
        return true;
      }
    }
  }
  return false;
}

TagResult Pn532Reader::readSpoolBytes(uint8_t out[qidiTagLength]) {
  if (_uidLength == 0) return TagResult::NoTag;
  if (_uidLength != 4) return TagResult::NotMifareClassic1k; // UL/DESFire etc.
  if (!authenticateSector1()) return TagResult::AuthenticationFailed;
  if (!nfc.mifareclassic_ReadDataBlock(qidiTagBlock, out)) {
    return TagResult::ReadFailed;
  }
  return TagResult::Ok;
}

TagResult Pn532Reader::writeSpoolBytes(const uint8_t data[qidiTagLength]) {
  if (_uidLength == 0) return TagResult::NoTag;
  if (_uidLength != 4) return TagResult::NotMifareClassic1k;
  if (!authenticateSector1()) return TagResult::AuthenticationFailed;
  if (!nfc.mifareclassic_WriteDataBlock(qidiTagBlock, (uint8_t *)data)) {
    return TagResult::WriteFailed;
  }
  uint8_t readBack[qidiTagLength];
  if (!nfc.mifareclassic_ReadDataBlock(qidiTagBlock, readBack) ||
      memcmp(readBack, data, qidiTagLength) != 0) {
    return TagResult::WriteFailed;
  }
  return TagResult::Ok;
}
