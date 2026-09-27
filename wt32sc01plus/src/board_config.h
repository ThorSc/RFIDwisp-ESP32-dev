#pragma once

// -----------------------------------------------------------------------
// WT32-SC01 Plus on-board display: ST7796, 320x480, 8-bit parallel (i80)
// interface - NOT SPI. Touch: FT6336 (FT5x06 protocol) on I2C.
// These are the values used by the community LovyanGFX configs for this
// board; if the display stays blank, compare against your board's schematic.
// -----------------------------------------------------------------------
#define TFT_WR 47
#define TFT_RD -1
#define TFT_RS 0 // DC
#define TFT_D0 9
#define TFT_D1 46
#define TFT_D2 3
#define TFT_D3 8
#define TFT_D4 18
#define TFT_D5 17
#define TFT_D6 16
#define TFT_D7 15
#define TFT_RST 4
#define TFT_BL 45

#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT 7
#define TOUCH_RST -1 // shares the display reset line

// Native panel orientation is portrait; the UI runs rotated to landscape.
#define PANEL_WIDTH 320
#define PANEL_HEIGHT 480
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// -----------------------------------------------------------------------
// PN532 wiring (external module, I2C mode) on a second I2C bus (Wire1),
// separate from the on-board touch controller's bus. GPIO 26-32 are used by
// flash/PSRAM on the ESP32-S3 and cannot be used; 10/11 are on the board's
// expansion header. Re-check against your board's silkscreen.
//
// PN532 module jumpers/switches must be set to I2C mode.
// -----------------------------------------------------------------------
#define PN532_SDA 10
#define PN532_SCL 11
#define PN532_IRQ -1   // not wired; polling mode is used
#define PN532_RESET -1 // not wired
