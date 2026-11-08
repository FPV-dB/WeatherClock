// ui.h — Weather Clock screens (240x240). Only depends on Adafruit_GFX: the firmware and the
// PC simulator (sim/) build this same code, so what the simulator shows
// is what appears on the device.
#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>
#include <time.h>

// Faces: the list lives in faces_list.h (the THEME_* enum is generated from it).
#include "faces_list.h"
enum UiTheme : uint8_t {
#define X(id, fn, name, desc) THEME_##id,
  UI_FACES(X)
#undef X
  THEME_COUNT
};
PGM_P uiFaceName(uint8_t t);  // name in the panel (in flash: copy with strncpy_P)
PGM_P uiFaceDesc(uint8_t t);  // short description (in flash, likewise)
enum UiPhase : uint8_t { PH_IDLE = 0, PH_WORK, PH_SHORT, PH_LONG };
// What the user does during focus: changes the drawing beside the tomato and the screen title.
enum UiActivity : uint8_t { ACT_READ = 0, ACT_STUDY, ACT_CODE, ACT_TASKS, ACT_DRAW, ACT_COUNT };

// One forecast day (today is the first)
struct UiDay {
  uint8_t code;        // WMO weather code
  int8_t tmin, tmax;   // °C, rounded
  uint8_t rain;        // chance of rain in %, 255 = no data
};
static const int UI_DAYS = 5;

struct UiWeather {
  bool ok;
  float t, tmin, tmax;
  int hum, code;
  bool day;
  uint16_t version;  // changes on every update: forces a weather redraw
  uint8_t nd;        // days in d[] (0 if the forecast didn't come)
  UiDay d[UI_DAYS];
  uint8_t rainIn;    // rain starting within the next hour: minutes until it (5 = any minute, 15, 30, 45); 0 = no
  uint8_t wind, gust;  // wind at 10 m, km/h (weatherbox)
  uint16_t wdir;       // direction it blows from, degrees
  // weatherbox: more Open-Meteo data
  float feels;         // apparent temperature
  uint8_t uv10;        // UV index x 10 (255 = no data)
  uint16_t aqi;        // US air quality index (0xFFFF = no data)
  int16_t rise, set;   // sunrise and sunset, minutes of the day (local); -1 = unknown
  uint8_t nh, h0;      // hours in ht/hp (next 12 h, starting with the current hour h0)
  int8_t ht[12];       // temperature per hour (rounded, screen unit)
  uint8_t hp[12];      // chance of rain per hour, %
};
static const int UI_HOURS = 12;

struct UiState {
  bool timeOk;
  struct tm now;
  bool hour12;
  // Pomodoro
  uint8_t phase;
  bool running;
  uint32_t remainingMs, totalMs;
  uint8_t cycleDone, longEvery;
  uint16_t today;
  uint8_t dailyGoal;
  uint8_t activity;  // UiActivity
  uint16_t streak;   // days in a row reaching the goal (includes today if already reached)
  uint16_t best;     // longest streak recorded
  // weather
  bool weatherOn;
  UiWeather wx;
  const char *city;
  bool fahr;  // wx temperatures already in °F (the faces just draw the number; see uiTempC)
  // general
  uint8_t theme;
  uint16_t accent;  // clock digit colour (RGB565)
  const char *ip;
  uint32_t ms;     // millis(): drives the animations
  bool celebrate;  // a Pomodoro phase just finished: plays the end animation
  bool south;      // weatherbox: southern hemisphere (the moon's lit side is mirrored)
};

constexpr uint16_t uiRgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// s.wx temperature in °C, for decisions (scarf, glasses) that must not change with the unit.
inline float uiTempC(const UiState &s, float v) { return s.fahr ? (v - 32) * 5 / 9 : v; }

// Pixel-art sprite. Sprites are generated from assets/*.txt by assets/generate.js into
// assets.h (SPR_<NAME>) and live in flash (PROGMEM), at 4 bits per pixel.
struct UiSprite {
  uint8_t w, h, frames;
  uint16_t frameMs;     // duration of each frame (animations)
  const uint16_t *pal;  // colour of indexes 1..15 (0 = transparent)
  const uint8_t *px;    // indexes, row by row, one frame after the other
};
static const int32_t UI_TRANSPARENT = -1;

// Draws frame "frame" with scale x scale pixels at (x, y). Transparent pixels
// stay as they are, or get "bg" (handy for repainting over the previous drawing).
void uiSprite(const UiSprite *s, int x, int y, uint8_t scale = 1, uint8_t frame = 0, int32_t bg = UI_TRANSPARENT);
uint8_t uiSpriteFrame(const UiSprite *s, uint32_t ms);  // animation frame at time ms

void uiBegin(Adafruit_GFX *gfx);
// Called between animation frames (e.g. the Flip turnover) to give time to Wi-Fi.
extern void (*uiFrameHook)();
void uiCornerClock(const char *hm);
// weatherbox: first-install screens, with a QR code for the phone
void uiSetOwner(const char *name);  // weatherbox: the clock's name, shown on the start-up screen ("" = none)
void uiSetupWifi(const char *ssid, const char *pass);     // join the clock's network
void uiSetupCity(const char *ip, const char *host);       // open the panel and choose the city  // weatherbox: current time in the bottom corner, on top of the radar
// Clock in ms for time-driven animations (firmware: millis). Without it, 20 ms steps.
extern uint32_t (*uiMillis)();
// Sends a block of RGB565 pixels to the screen. The firmware points this at the display's fast
// method (one SPI transaction per block); without it, it falls back to Adafruit_GFX's generic one, pixel by pixel.
extern void (*uiBlit)(int16_t x, int16_t y, uint16_t *px, int16_t w, int16_t h);
// Active clip (face packs redraw only the area that changed). The firmware honours
// this when drawing JPEG images itself.
struct UiClip { bool on; int16_t x0, y0, x1, y1; };
extern UiClip uiClip;
void uiClipBegin(int x0, int y0, int x1, int y1);
void uiClipEnd();
// Draws the area (x, y, w, h) off-screen, in strips of a few KB, and sends each strip finished to the
// screen in one go: the screen never shows the cleared background between "clear" and "draw again" (which
// flickered on the hands). draw(ctx) draws the whole scene into G, which clips to the current strip; it may
// be called several times. Returns false with no memory for the strip (then the caller draws directly).
bool uiOffscreen(int x, int y, int w, int h, void (*draw)(void *), void *ctx);
void uiInvalidate();                         // redraw everything on the next call
// Same as uiInvalidate(), but, if a firmware face is on screen and the next one is too, the
// change is a vertical hardware scroll (the old one moves up and out, the new one comes in from below) instead of
// clearing and redrawing in front of the viewer. Without uiScrollHw (simulator), it becomes uiInvalidate().
void uiInvalidateSlide();

// ---- vertical hardware scrolling (ST7789: VSCRDEF 0x33 and VSCSAD 0x37) ----
// The panel's RAM has 320 rows and the glass shows 240. Scrolling just changes where the panel starts
// reading RAM: 4 bytes over SPI, zero CPU per frame (scrolling in software would resend 115 KB per frame).
// The 80 RAM rows the glass doesn't show (the "margin") become the gap the content passes through; the
// firmware blacks them out when setting up the panel (applyPanel). The whole screen scrolls: the chip's fixed
// TFA/BFA bands are no use, because the visible rows are always the first 240 read.
//   rows   = rows in the scroll ring, 241..UI_PANEL_ROWS (320 uses all the RAM)
//   offset = how many rows the content has moved up the screen (0 = still), taken modulo rows
// The content at y = 0..239 moves up and out the top; what leaves comes back from below after the gap, so
// the RAM rows of y = 0..239 can take the new drawing as soon as they're out of view.
const int UI_PANEL_ROWS = 320;
// Firmware hook (rows = 0 switches it off: the chip's default, offset 0). The simulator models the panel.
extern void (*uiScrollHw)(int16_t rows, int16_t offset);
bool uiScrollBegin(int rows);   // false: not supported or invalid rows (nothing changes)
void uiScrollTo(int offset);
void uiScrollEnd();             // always returns to offset 0; harmless if nothing is scrolling
bool uiScrollActive();
// VSCSAD value (the RAM row where reading starts) for a screen offset. With "rotate 180°"
// (MADCTL MY) the RAM is mirrored relative to what the viewer sees, so the direction flips:
//   normal: displayed row n shows RAM row n + SAD, the content moves up with SAD = offset
//   rotated: SAD = (rows - offset) % rows
inline uint16_t uiScrollSad(bool flip, int rows, int offset) {
  return (uint16_t)(flip && offset ? rows - offset : offset);
}
void uiSetLang(uint8_t lang);                // screen texts: 0 Portuguese, 1 English, 2 Spanish
extern uint8_t uiLang;
// tr("pt", "en", "es"): all three texts live in flash (PSTR) and the current language's is copied into one
// of 6 rotating 48-byte buffers (RAM is scarce: the album's GIF needs ~22 KB contiguous).
// The pointer stays valid for up to 5 more tr() calls; anyone keeping the text must copy it.
const char *trP(PGM_P pt, PGM_P en, PGM_P es);
#define tr(pt, en, es) trP(PSTR(pt), PSTR(en), PSTR(es))
// A shared 240-pixel row aligned to 4 bytes: the aligned copy SPI requires (tftPixels), the
// GIF drawing and the Flip row. Never two uses at the same time (one buffer instead of three: ~700 bytes).
uint16_t *uiRowBuf();
extern uint32_t uiEnters;                    // counter of whole-screen changes
void uiRenderClock(const UiState &s);        // clock screen in the chosen theme
void uiRenderPomodoro(const UiState &s);     // Pomodoro progress ring
void uiMessage(const char *title, const char *l1, const char *l2, const char *l3);
void uiMessageLine(const char *l1);              // changes only the highlighted line of the message on screen
void uiBootProgress(uint32_t ms, uint8_t done);  // bar and steps of the start-up screen (only with uiMessage on screen)
void uiSyncHint(uint32_t ms);                    // "syncing" over the face, while there's no time
void uiColorTest();                          // R/G/B/white bars to tune the panel
// default VCOM: 0x1E = 0.850 V (with 0.900 V, the chip's reset value, the SmallTV Ultra showed ghosting)
static const uint8_t UI_VCOM_DEFAULT = 0x1E;
void uiVcomTest(uint8_t vcom, bool full);    // anti-ghosting tuning patterns (full: draw everything; otherwise just the value)
void uiError(const char *msg);               // centred notice (e.g. invalid GIF)
