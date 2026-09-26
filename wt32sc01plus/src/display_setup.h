#pragma once
#include <lvgl.h>

// Brings up the ST7796 panel + FT6336 touch over LovyanGFX and wires them
// into LVGL as a single display/input driver pair. Call once from setup().
void displaySetup();

// Must be called periodically (e.g. every loop() iteration) to keep LVGL's
// tick and task handler running.
void displayLoop();
