#pragma once

// -----------------------------------------------------------------------
// WT32-SC01 Plus on-board display (ST7796, 320x480) and touch (FT6336, I2C).
// These are the pin assignments used by the vendor's own demo firmware for
// the "Plus" revision. Different production batches have been shipped with
// different silkscreens - if the display stays blank or touch does not
// respond, check the pinout printed on the board (or the seller's demo
// project) against the values below before changing anything else.
// -----------------------------------------------------------------------
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 21
#define TFT_RST -1 // tied to the board's own reset
#define TFT_BL 23

#define TOUCH_SDA 18
#define TOUCH_SCL 19
#define TOUCH_RST 38
#define TOUCH_INT 39

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480

// -----------------------------------------------------------------------
// PN532 wiring (external module, I2C mode).
//
// The on-board I2C bus above is already used for the touch controller, so
// the PN532 is wired to a second, bit-banged I2C bus on two free GPIOs from
// the WT32-SC01 Plus's expansion header. The pins below (bottom header,
// unused by the display/touch/SD) are a starting point - re-check them
// against your board's silkscreen, since header pinout has varied between
// batches.
//
// PN532 module jumpers/switches must be set to I2C mode.
// -----------------------------------------------------------------------
#define PN532_SDA 32
#define PN532_SCL 33
#define PN532_IRQ 27  // set to -1 if not wired; polling mode is used either way
#define PN532_RESET 26
