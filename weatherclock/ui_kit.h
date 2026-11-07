// ui_kit.h — drawing kit for the faces (internal use by ui.cpp and face_*.cpp).
//
// A face is a function  void themeX(const UiState &s, bool full)  called every ~100 ms.
// full = true on the first call (screen already cleared with BG): draw everything. On the following ones,
// redraw only what changed, using the caches below. Full guide: docs/MAKING_A_FACE.md
#pragma once
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------- screen and constants ----------
extern Adafruit_GFX *G;  // the screen (in the simulator, a framebuffer)
static const float K_PI = 3.14159265f;

// paleta comum
static const uint16_t BG     = uiRgb(0, 0, 0);
static const uint16_t CARD   = uiRgb(22, 22, 26);
static const uint16_t CARD2  = uiRgb(44, 44, 50);
static const uint16_t TEXT   = uiRgb(242, 242, 242);
static const uint16_t MUTED  = uiRgb(125, 125, 135);
static const uint16_t DIM    = uiRgb(30, 30, 34);
static const uint16_t WORK   = uiRgb(255, 90, 71);
static const uint16_t SHORT  = uiRgb(62, 207, 122);
static const uint16_t LONG   = uiRgb(79, 141, 255);
static const uint16_t SUN    = uiRgb(255, 196, 40);
static const uint16_t CLOUD  = uiRgb(214, 220, 230);
static const uint16_t DCLOUD = uiRgb(120, 126, 138);
static const uint16_t RAIN   = uiRgb(90, 170, 255);

// ---------- idioma (uiSetLang) ----------
extern char WD[7][4];   // abbreviated weekday in the current language ("TER", "TUE", "MAR")
extern char MO[12][4];  // abbreviated month ("OUT", "OCT", "OCT")

// ---------- text (built-in 6x8 font, integer scale; ASCII only) ----------
int textW(const char *s, uint8_t sz);
void text(int x, int y, uint8_t sz, uint16_t c, const char *s);
void textC(int cx, int y, uint8_t sz, uint16_t c, const char *s);  // centred on cx
void textR(int rx, int y, uint8_t sz, uint16_t c, const char *s);  // right-aligned at rx
void degree(int x, int y, uint8_t sz, uint16_t c);
// weatherbox: clears the box and writes the text off-screen, in a single send: changes without flicker.
// align: 'l' left (tx = start), 'r' right (tx = end), 'c' centre (tx = middle)
void uiTextBox(int x, int y, int w, int h, uint16_t bg, int tx, int ty, uint8_t sz, uint16_t c, const char *s, char align = 'l');                 // degree sign drawn by hand
void tempText(int cx, int y, uint8_t sz, uint16_t c, float v);     // "23°" centralizado
void tempDesc(int cy, uint8_t sz, uint16_t ct, uint16_t cd, float t, const char *desc);  // "23° Rain"

// ---------- flicker-free redraw ----------
// draw(ctx) in the area (x, y, w, h): off-screen (uiOffscreen) or, with direct (whole screen at once)
// or with no memory for the strip, straight to the screen.
void uiPaint(bool direct, int x, int y, int w, int h, void (*draw)(void *), void *ctx);

// ---------- formas ----------
void thickLine(float x0, float y0, float x1, float y1, float w, uint16_t c);  // pontas redondas
uint16_t mix(uint16_t a, uint16_t b, float f);                                  // f=0 -> a, f=1 -> b
void blit(int16_t x, int16_t y, uint16_t *px, int16_t w, int16_t h);            // RGB565 block (fast)

// ---------- digits ----------
// Solid 5x7 pixel art: p = cell size (digit = 5p x 7p); d < 0 blanks it; DM_MINUS = "-".
static const int DM_MINUS = 10;
void dmDigit(int x, int y, int d, int p, uint16_t on, uint16_t off, bool drawOff);
// HH:MM in pixel art with incremental redraw (uses lastDig/lastColon)
struct TimeLayout { int x[4]; int y; int p; int colonX; };
void drawTime4(const TimeLayout &L, int a, int b, int c, int d, bool colon, uint16_t on, uint16_t off);
// DSEG7 (LCD clock): 0.618h x h digit; unlit segments in "off"; d = 10 draws "-", d < 0 only the unlit ones
void dsegDigit(int x, int y, int h, int d, uint16_t on, uint16_t off);
// Smoothed vector fonts over bg (digits, ':' and space only), height h (fonts.cpp)
struct VecFont {
  const int16_t (*pt)[2];  // outline points (720-high cell)
  const uint16_t *cend;    // end (exclusive) of each contour
  const uint8_t *glyph;    // first contour of 0-9 and ':' (+ end)
  const uint16_t *adv;     // advance of 0-9 and ':'
  const char *chars = nullptr;  // text font: characters in glyph/adv order (PROGMEM); nullptr = digits only
};
extern const VecFont VF_BEBAS, VF_OUTFIT, VF_RIGHTEOUS;
// Outfit with letters, digits, space, ':', '-' and '%': the 720 cell = top of the capitals to the bottom
// of the descenders (h is the whole line's height). Characters not in the list advance half a space.
extern const VecFont VF_OUTFIT_TXT;
void vecText(const VecFont &f, int x, int y, int h, const char *s, uint16_t fg, uint16_t bg);
int vecWidth(const VecFont &f, const char *s, int h);
uint16_t mix16(uint16_t a, uint16_t b, int n);  // n/16 of b over a
// "Geo" vector font: 6s x 10s digit with a stroke of thickness w
void geoDigit(float x, float y, int d, float s, float w, uint16_t c);

// ---------- time and date ----------
int displayHour(const UiState &s);                       // respeita o formato 12 h
void dateLabel(const UiState &s, char *out, size_t n);   // "TER 20 OUT"
void statusBar(const UiState &s, const char *right, uint16_t bg);  // date on the left, text on the right

// ---------- weather ----------
void weatherIcon(int cx, int cy, float s, int code, bool day, uint16_t bg, uint32_t ms = 0);  // s=1 -> ~40 px; ms>0 anima
const char *weatherText(int code);                                          // "Rain", "Cloudy"...
const char *rainSoonText(uint8_t minutes);  // "rain 15 min" / "rain soon" (UiWeather::rainIn); "" if 0
void cloudShape(int cx, int cy, float s, uint16_t c);
void sunShape(int cx, int cy, float s);
void moonShape(int cx, int cy, float s, uint16_t bg);

// ---------- mascote (sprites de assets/, ver assets.h) ----------
enum Face : uint8_t { FACE_HAPPY, FACE_SLEEP, FACE_FOCUS, FACE_RELAX, FACE_CHEER, FACE_UMBRELLA, FACE_SCARF, FACE_SHADES };
void mascot(int x, int y, int sc, uint8_t face, int32_t bg = UI_TRANSPARENT);  // tomate 16x16 x sc
// Tomato plant by streak: seed, sprout, flower, green tomato, ripe and golden (7+ days)
const UiSprite *plantSprite(uint16_t streak);
void streakBadge(int x, int y, uint16_t streak, uint16_t c);  // flame + number; nothing if streak == 0

// ---------- redesenho incremental ----------
// Each area remembers what it showed and only repaints when it changes. Everything resets on a screen change.
struct Cache { char s[40]; };
bool changed(Cache &c, const char *now);  // true (and stores it) if the text changed
extern Cache cStatL, cStatR, cA, cB, cC;
extern int8_t lastDig[4], lastColon;
extern uint16_t lastDigCol;
extern int lastSecW, lastMin, lastHour, lastToday, lastGoal, lastRing, lastDots, lastWxOn;
extern int8_t lastFace;
// weatherbox: wind direction (degrees) as 16 compass points
static inline const char *windDir(int deg) {
  static const char D[16][4] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  return D[((deg * 2 + 22) / 45) % 16];
}
extern uint16_t lastWx;  // compare with s.wx.version to know whether the weather changed

// ---------- registered faces (faces_list.h) ----------
#define X(id, fn, name, desc) void fn(const UiState &s, bool full);
UI_FACES(X)
#undef X
