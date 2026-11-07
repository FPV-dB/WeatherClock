// Minimal Arduino shim to build ui.cpp + Adafruit_GFX on the PC (simulator).
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#ifndef ARDUINO
#define ARDUINO 100
#endif
typedef bool boolean;
typedef uint8_t byte;
#include "Print.h"
#include "pgmspace.h"
#ifdef _WIN32
static inline struct tm *localtime_r(const time_t *t, struct tm *r) { return localtime_s(r, t) ? nullptr : r; }
#endif
// Adafruit_GFX only uses String/__FlashStringHelper in getTextBounds; the bare minimum is enough.
class __FlashStringHelper;
class String {
  const char *s;
 public:
  String(const char *x = "") : s(x) {}
  const char *c_str() const { return s; }
  unsigned length() const { return strlen(s); }
};
#define radians(d) ((d) * 0.017453292519943295)
// PROGMEM on the PC: ordinary memory
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_word
#define pgm_read_word(a) (*(const uint16_t *)(a))
#endif
