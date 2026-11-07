// ui.cpp — the visual system for the Weather Clock's 240x240 screen (base: the SmallTV Pomodoro "Tomato" style).
//
// Principles (designed for a 1.54" IPS seen from ~1 m):
//  - pure black background (the IPS gets really dark) and rounded dark-grey cards;
//  - numbers in solid pixel art (same look as the mascot): thick, continuous strokes,
//    readable from afar; no flicker, because every cell is always repainted (colour or background);
//  - only the built-in 6x8 font, at integer scale (sharp pixels, nothing blurry);
//  - incremental redraw: each area remembers what it showed and only repaints if it changed.

#include "ui.h"
#include "assets.h"
#include "ui_kit.h"
#include "facepack.h"

Adafruit_GFX *G = nullptr;
#include <math.h>
#include <stdio.h>
#include <string.h>


// ---------- language ----------
static const char WD_L[3][7][4] PROGMEM = {{"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"},
                                           {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"},
                                           {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"}};
static const char MO_L[3][12][4] PROGMEM = {{"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"},
                                            {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"},
                                            {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"}};
static uint16_t rowBuf[240] __attribute__((aligned(4)));
uint16_t *uiRowBuf() { return rowBuf; }
uint8_t uiLang = 1;
char WD[7][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};  // English until uiSetLang
char MO[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
void uiSetLang(uint8_t lang) {
  uiLang = lang < 3 ? lang : 1;
  memcpy_P(WD, WD_L[uiLang], sizeof WD);
  memcpy_P(MO, MO_L[uiLang], sizeof MO);
  uiInvalidate();
}
const char *trP(PGM_P pt, PGM_P en, PGM_P es) {
  static char buf[6][48];
  static uint8_t k = 0;
  char *b = buf[k++ % 6];
  strncpy_P(b, uiLang == 1 ? en : uiLang == 2 ? es : pt, sizeof buf[0] - 1);
  b[sizeof buf[0] - 1] = 0;
  return b;
}
// labels inside the Pomodoro ring: up to 11 characters
static const char *phaseLabel(uint8_t p) {
  return p == PH_WORK ? tr("FOCO", "FOCUS", "FOCO") : p == PH_SHORT ? tr("PAUSA", "BREAK", "PAUSA") : p == PH_LONG ? tr("PAUSA LONGA", "LONG BREAK", "PAUSA LARGA") : "";
}

// ---------- text (built-in 6x8 font) ----------
int textW(const char *s, uint8_t sz) {
  int n = strlen(s);
  return n ? n * 6 * sz - sz : 0;
}
void text(int x, int y, uint8_t sz, uint16_t c, const char *s) {
  G->setTextSize(sz);
  G->setTextColor(c);
  G->setCursor(x, y);
  G->print(s);
}
void textC(int cx, int y, uint8_t sz, uint16_t c, const char *s) { text(cx - textW(s, sz) / 2, y, sz, c, s); }
void textR(int rx, int y, uint8_t sz, uint16_t c, const char *s) { text(rx - textW(s, sz), y, sz, c, s); }

void degree(int x, int y, uint8_t sz, uint16_t c) {
  int r = sz + 1;
  G->drawCircle(x + r, y + r, r, c);
  if (sz >= 3) G->drawCircle(x + r, y + r, r - 1, c);
}
// "23°" centred on cx
void tempText(int cx, int y, uint8_t sz, uint16_t c, float v) {
  char b[8];
  snprintf(b, sizeof b, "%d", (int)lroundf(v));
  int w = textW(b, sz) + sz + 2 * (sz + 1) + 2;
  int x = cx - w / 2;
  text(x, y, sz, c, b);
  degree(x + textW(b, sz) + sz + 1, y, sz, c);
}

// Thick line with rounded ends (two triangles + two circles).
void thickLine(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), r = w / 2;
  if (len > 0.01f) {
    float nx = -dy / len * r, ny = dx / len * r;
    G->fillTriangle(lroundf(x0 + nx), lroundf(y0 + ny), lroundf(x1 + nx), lroundf(y1 + ny), lroundf(x1 - nx), lroundf(y1 - ny), c);
    G->fillTriangle(lroundf(x0 + nx), lroundf(y0 + ny), lroundf(x1 - nx), lroundf(y1 - ny), lroundf(x0 - nx), lroundf(y0 - ny), c);
  }
  if (r >= 1) {
    G->fillCircle(lroundf(x0), lroundf(y0), lroundf(r), c);
    G->fillCircle(lroundf(x1), lroundf(y1), lroundf(r), c);
  }
}

// ---------- area cache ----------
bool changed(Cache &c, const char *now) {
  if (strcmp(c.s, now) == 0) return false;
  strncpy(c.s, now, sizeof c.s - 1);
  c.s[sizeof c.s - 1] = 0;
  return true;
}

// Screen in front: the face number (0..THEME_COUNT-1) or one of the screens below. The other screens'
// numbers stay well away from the faces: up to 1.19 the Pomodoro was 10, the same as the 11th face (Forecast LCD),
// and switching between the two didn't clear the screen (one appeared on top of the other).
enum : int8_t { SCR_PACK = 100, SCR_POMO, SCR_MSG, SCR_COLOR, SCR_ERROR, SCR_VCOM };
static_assert(THEME_COUNT < SCR_PACK, "a face number would collide with another screen's");
static int8_t screen = -1;
Cache cStatL, cStatR, cA, cB, cC;
int8_t lastDig[4], lastColon;
uint16_t lastDigCol;
int lastSecW, lastMin, lastHour, lastToday, lastGoal, lastRing, lastDots, lastWxOn;
int8_t lastFace;
uint16_t lastWx;

static void resetCaches() {
  cStatL.s[0] = cStatR.s[0] = cA.s[0] = cB.s[0] = cC.s[0] = 1;  // impossible value
  cStatL.s[1] = cStatR.s[1] = cA.s[1] = cB.s[1] = cC.s[1] = 0;
  for (auto &d : lastDig) d = -2;
  lastColon = -1;
  lastDigCol = 0xFFFF;
  lastSecW = lastMin = lastHour = lastToday = lastGoal = lastRing = lastDots = lastWxOn = -1;
  lastFace = -1;
  lastWx = 0xFFFF;
}

uint32_t uiEnters = 0;  // how many times the whole screen was changed (main.cpp times that drawing)
static bool enter(int8_t id) {
  if (screen == id) return false;
  screen = id;
  uiEnters++;
  uiScrollEnd();
  resetCaches();
  G->fillScreen(BG);
  return true;
}

void uiBegin(Adafruit_GFX *gfx) {
  G = gfx;
  G->setTextWrap(false);
  uiScrollEnd();
  screen = -1;
}
static bool slideOn = false;  // the next full screen (screen == -1) replaces a firmware face
void uiInvalidate() {
  screen = -1;
  slideOn = false;
}
void uiInvalidateSlide() {
  bool was = screen >= 0 && screen < THEME_COUNT;
  screen = -1;
  slideOn = was;
}

// ---------- 5x7 pixel-art digits (solid cells) ----------
static const uint8_t DM[11][7] PROGMEM = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, {0x1F, 0x01, 0x02, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},  // 10 = minus sign
};

// p = cell size; d < 0 = blank digit. Lit cells are filled squares
// (continuous stroke); with drawOff, unlit ones get "off" (the background colour), which
// replaces the previous digit without clearing the area first (no flicker).
void dmDigit(int x, int y, int d, int p, uint16_t on, uint16_t off, bool drawOff) {
  for (int row = 0; row < 7; row++) {
    uint8_t bits = d < 0 ? 0 : pgm_read_byte(&DM[d][row]);
    for (int col = 0; col < 5;) {
      bool lit = bits & (0x10 >> col);
      int run = 1;  // merges equal neighbouring cells into a single rectangle (fewer SPI calls)
      while (col + run < 5 && (bool)(bits & (0x10 >> (col + run))) == lit) run++;
      if (lit || drawOff) G->fillRect(x + col * p, y + row * p, run * p, p, lit ? on : off);
      col += run;
    }
  }
}


void drawTime4(const TimeLayout &L, int a, int b, int c, int d, bool colon, uint16_t on, uint16_t off) {
  int v[4] = {a, b, c, d};
  bool colChanged = on != lastDigCol;
  for (int i = 0; i < 4; i++)
    if (colChanged || lastDig[i] != v[i]) {
      dmDigit(L.x[i], L.y, v[i], L.p, on, off, true);
      lastDig[i] = v[i];
    }
  if (colChanged || lastColon != colon) {
    uint16_t cc = colon ? on : off;
    G->fillRect(L.colonX, L.y + 2 * L.p, L.p, L.p, cc);
    G->fillRect(L.colonX, L.y + 4 * L.p, L.p, L.p, cc);
    lastColon = colon;
  }
  lastDigCol = on;
}

int displayHour(const UiState &s) {
  int h = s.now.tm_hour;
  if (s.hour12) h = h % 12 == 0 ? 12 : h % 12;
  return h;
}

void dateLabel(const UiState &s, char *out, size_t n) {
  if (s.timeOk) snprintf(out, n, "%s %02d %s", WD[s.now.tm_wday], s.now.tm_mday, MO[s.now.tm_mon]);
  else snprintf(out, n, "%s", tr("SINCRONIZANDO", "SYNCING", "SINCRONIZANDO"));
}

// Top bar: date on the left, free text (IP/time) on the right.
void statusBar(const UiState &s, const char *right, uint16_t bg) {
  char l[24];
  dateLabel(s, l, sizeof l);
  if (changed(cStatL, l)) {
    uiTextBox(0, 4, 120, 10, bg, 10, 6, 1, MUTED, l, 'l');
  }
  if (changed(cStatR, right)) {
    uiTextBox(120, 4, 120, 10, bg, 230, 6, 1, MUTED, right, 'r');
  }
}

struct TextBoxJob { int x, y, w, h; uint16_t bg; int tx, ty; uint8_t sz; uint16_t c; const char *s; char align; };
static void paintTextBox(void *p) {
  const TextBoxJob &j = *(const TextBoxJob *)p;
  G->fillRect(j.x, j.y, j.w, j.h, j.bg);
  if (j.align == 'r') textR(j.tx, j.ty, j.sz, j.c, j.s);
  else if (j.align == 'c') textC(j.tx, j.ty, j.sz, j.c, j.s);
  else text(j.tx, j.ty, j.sz, j.c, j.s);
}
void uiTextBox(int x, int y, int w, int h, uint16_t bg, int tx, int ty, uint8_t sz, uint16_t c, const char *s, char align) {
  TextBoxJob j = {x, y, w, h, bg, tx, ty, sz, c, s, align};
  uiPaint(false, x, y, w, h, paintTextBox, &j);
}

// ---------- weather icons ----------
void cloudShape(int cx, int cy, float s, uint16_t c) {
  G->fillCircle(cx - 8 * s, cy + 3 * s, 8 * s, c);
  G->fillCircle(cx + 2 * s, cy - 3 * s, 11 * s, c);
  G->fillCircle(cx + 12 * s, cy + 4 * s, 7 * s, c);
  G->fillRoundRect(cx - 16 * s, cy + 3 * s, 35 * s, 9 * s, 4 * s, c);
}
static float sunSpin = 0;  // weatherbox: rotation of the rays (radians) for the animated icon
void sunShape(int cx, int cy, float s) {
  for (int i = 0; i < 8; i++) {
    float a = i * K_PI / 4 + sunSpin;
    int x0 = cx + cosf(a) * 13 * s, y0 = cy + sinf(a) * 13 * s;
    int x1 = cx + cosf(a) * 18 * s, y1 = cy + sinf(a) * 18 * s;
    for (int k = -1; k <= 1; k++) G->drawLine(x0 + k, y0, x1 + k, y1, SUN);
  }
  G->fillCircle(cx, cy, 9 * s, SUN);
}
void moonShape(int cx, int cy, float s, uint16_t bg) {
  G->fillCircle(cx, cy, 12 * s, uiRgb(244, 236, 200));
  G->fillCircle(cx + 6 * s, cy - 4 * s, 10 * s, bg);
}
// Draws over an area already cleared with "bg". With ms > 0 (weatherbox) the icon animates: rays turning, cloud
// drifting back and forth, rain and snow falling, fog sliding and lightning flashing. With ms = 0 it stays as before.
void weatherIcon(int cx, int cy, float s, int c, bool day, uint16_t bg, uint32_t ms) {
  bool rain = (c >= 51 && c <= 67) || (c >= 80 && c <= 82);
  bool snow = (c >= 71 && c <= 77) || c == 85 || c == 86;
  bool storm = c >= 95;
  const float t = ms / 1000.0f;
  const float drift = ms ? sinf(t * 1.6f) * 2.5f * s : 0;  // cloud sways sideways
  sunSpin = ms ? t * 0.6f : 0;
  if (c == 0) {
    if (day) sunShape(cx, cy, s);
    else moonShape(cx, cy, s, bg);
  } else if (c <= 2) {
    if (day) sunShape(cx - 7 * s, cy - 7 * s, s * 0.75f);
    else moonShape(cx - 7 * s, cy - 7 * s, s * 0.75f, bg);
    cloudShape(cx + 3 * s + drift, cy + 4 * s, s * 0.8f, CLOUD);
  } else if (c == 45 || c == 48) {
    for (int i = 0; i < 4; i++) {
      const float dx = ms ? sinf(t * 1.2f + i * 1.3f) * 3 * s : 0;
      G->fillRoundRect(cx - 18 * s + dx, cy - 10 * s + i * 7 * s, 36 * s, 3 * s + 1, 2, CLOUD);
    }
  } else {
    // rain and snow fall in a 10*s px cycle, under the cloud
    const float fall = ms ? fmodf(t * 18 * s, 10 * s) : 0;
    if (rain)
      for (int i = -1; i <= 1; i++)
        for (int k = 0; k < 2; k++) {
          const float y0 = cy + 8 * s + fmodf(fall + (i + 1) * 3.3f * s, 10 * s);
          G->drawLine(cx + i * 9 * s + k, y0, cx + i * 9 * s - 4 * s + k, y0 + 8 * s, RAIN);
        }
    if (snow)
      for (int i = -1; i <= 1; i++)
        G->fillCircle(cx + i * 9 * s + (ms ? sinf(t * 2 + i) * 2 * s : 0), cy + 12 * s + fmodf(fall + (i + 1) * 3.3f * s, 10 * s), 2 * s + 1, TEXT);
    cloudShape(cx + drift, cy - 4 * s, s, storm || rain ? DCLOUD : CLOUD);
    if (storm && (!ms || (ms / 150) % 10 < 3)) {  // flashes for ~30% of the time
      G->fillTriangle(cx + 2 * s, cy + 6 * s, cx - 6 * s, cy + 16 * s, cx + 1 * s, cy + 15 * s, SUN);
      G->fillTriangle(cx - 1 * s, cy + 13 * s, cx + 6 * s, cy + 13 * s, cx - 4 * s, cy + 24 * s, SUN);
    }
  }
  sunSpin = 0;
}

const char *rainSoonText(uint8_t m) {
  static char b[20];
  if (!m) return "";
  if (m < 15) return tr("chuva logo", "rain soon", "lluvia ya");
  snprintf(b, sizeof b, tr("chuva %u min", "rain %u min", "lluvia %u min"), m);  // short: fits beside the tomato
  return b;
}

const char *weatherText(int c) {
  if (c == 0) return tr("Ceu limpo", "Clear sky", "Despejado");
  if (c <= 2) return tr("Poucas nuvens", "Few clouds", "Poco nuboso");
  if (c == 3) return tr("Nublado", "Cloudy", "Nublado");
  if (c == 45 || c == 48) return tr("Neblina", "Fog", "Niebla");
  if (c >= 51 && c <= 57) return tr("Garoa", "Drizzle", "Llovizna");
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return tr("Chuva", "Rain", "Lluvia");
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return tr("Neve", "Snow", "Nieve");
  if (c >= 95) return tr("Tempestade", "Storm", "Tormenta");
  return "";
}

// ---------- pixel-art sprites (assets/*.txt -> assets.h) ----------
void uiSprite(const UiSprite *sp, int x, int y, uint8_t sc, uint8_t frame, int32_t bg) {
  UiSprite s;
  memcpy_P(&s, sp, sizeof s);
  const uint8_t *px = s.px + (frame % s.frames) * ((s.w * s.h + 1) / 2);
  auto at = [&](int i) { uint8_t b = pgm_read_byte(px + i / 2); return i & 1 ? b & 15 : b >> 4; };
  for (int r = 0; r < s.h; r++)
    for (int c = 0; c < s.w;) {
      int i = r * s.w + c, v = at(i), run = 1;  // merges equal neighbouring pixels into a single rectangle
      while (c + run < s.w && at(i + run) == v) run++;
      if (v || bg >= 0) G->fillRect(x + c * sc, y + r * sc, run * sc, sc, v ? pgm_read_word(s.pal + v - 1) : (uint16_t)bg);
      c += run;
    }
}

uint8_t uiSpriteFrame(const UiSprite *sp, uint32_t ms) {
  UiSprite s;
  memcpy_P(&s, sp, sizeof s);
  return s.frames > 1 && s.frameMs ? ms / s.frameMs % s.frames : 0;
}

// Mascot: 16x16 tomato with one expression per asset.
static const UiSprite *const FACES[] PROGMEM = {&SPR_TOMATO_HAPPY, &SPR_TOMATO_SLEEPING, &SPR_TOMATO_FOCUSED, &SPR_TOMATO_RELAXED,
                                        &SPR_TOMATO_CHEERING, &SPR_TOMATO_UMBRELLA, &SPR_TOMATO_SCARF, &SPR_TOMATO_GLASSES};

void mascot(int x, int y, int sc, uint8_t face, int32_t bg) { uiSprite((const UiSprite *)pgm_read_ptr(&FACES[face]), x, y, sc, 0, bg); }

const UiSprite *plantSprite(uint16_t streak) {
  static const UiSprite *const STAGE[8] PROGMEM = {&SPR_PLANT_SEED, &SPR_PLANT_SPROUT, &SPR_PLANT_FLOWER, &SPR_PLANT_FLOWER,
                                           &SPR_PLANT_GREEN, &SPR_PLANT_GREEN, &SPR_PLANT_RIPE, &SPR_PLANT_GOLDEN};
  return (const UiSprite *)pgm_read_ptr(&STAGE[streak < 7 ? streak : 7]);
}

void streakBadge(int x, int y, uint16_t streak, uint16_t c) {
  if (!streak) return;
  char n[6];
  snprintf(n, sizeof n, "%u", streak);
  uiSprite(&SPR_FLAME, x, y, 1);
  text(x + 10, y + 2, 1, c, n);
}

// Focus activities (UiActivity): object beside the tomato and the Pomodoro screen's title.
static const UiSprite *const ACTIVITY_SPRITE[ACT_COUNT] PROGMEM = {&SPR_BOOK, &SPR_JOTTER, &SPR_NOTEBOOK, &SPR_CLIPBOARD, &SPR_PAINT_PALETTE};
static const char *activityLabel(uint8_t a) {
  switch (a) {
    case 1: return tr("ESTUDANDO", "STUDYING", "ESTUDIANDO");
    case 2: return tr("PROGRAMANDO", "CODING", "PROGRAMANDO");
    case 3: return tr("TAREFAS", "TASKS", "TAREAS");
    case 4: return tr("DESENHANDO", "DRAWING", "DIBUJANDO");
  }
  return tr("LENDO", "READING", "LEYENDO");
}

void (*uiFrameHook)() = nullptr;     // the firmware uses it to give time to Wi-Fi between frames
uint32_t (*uiMillis)() = nullptr;    // clock in ms (firmware: millis); without it, fixed 20 ms steps
void (*uiBlit)(int16_t, int16_t, uint16_t *, int16_t, int16_t) = nullptr;

// ---------- vertical hardware scrolling (see ui.h) ----------
void (*uiScrollHw)(int16_t, int16_t) = nullptr;
static int16_t scrollRows = 0;  // 0 = still
bool uiScrollBegin(int rows) {
  if (!uiScrollHw || rows <= 240 || rows > UI_PANEL_ROWS) return false;
  scrollRows = rows;
  uiScrollHw(rows, 0);
  return true;
}
void uiScrollTo(int offset) {
  if (!scrollRows) return;
  offset %= scrollRows;
  if (offset < 0) offset += scrollRows;
  uiScrollHw(scrollRows, offset);
}
void uiScrollEnd() {
  if (!scrollRows) return;
  scrollRows = 0;
  uiScrollHw(0, 0);
}
bool uiScrollActive() { return scrollRows != 0; }

// ---------- clipping ----------
// Face packs redraw only the area that changed: G becomes this wrapper, which
// discards whatever falls outside the rectangle and passes the rest on to the real screen.
UiClip uiClip = {false, 0, 0, 0, 0};
static Adafruit_GFX *realG = nullptr;

class ClipGfx : public Adafruit_GFX {
 public:
  Adafruit_GFX *t = nullptr;
  ClipGfx() : Adafruit_GFX(240, 240) {}
  static bool in(int16_t x, int16_t y) { return x >= uiClip.x0 && x < uiClip.x1 && y >= uiClip.y0 && y < uiClip.y1; }
  static bool rect(int16_t &x, int16_t &y, int16_t &w, int16_t &h) {
    if (x < uiClip.x0) { w -= uiClip.x0 - x; x = uiClip.x0; }
    if (y < uiClip.y0) { h -= uiClip.y0 - y; y = uiClip.y0; }
    if (x + w > uiClip.x1) w = uiClip.x1 - x;
    if (y + h > uiClip.y1) h = uiClip.y1 - y;
    return w > 0 && h > 0;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { if (in(x, y)) t->drawPixel(x, y, c); }
  void writePixel(int16_t x, int16_t y, uint16_t c) override { if (in(x, y)) t->writePixel(x, y, c); }
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override { if (rect(x, y, w, h)) t->writeFillRect(x, y, w, h, c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override { if (rect(x, y, w, h)) t->fillRect(x, y, w, h, c); }
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { int16_t h = 1; if (rect(x, y, w, h)) t->writeFastHLine(x, y, w, c); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { int16_t h = 1; if (rect(x, y, w, h)) t->drawFastHLine(x, y, w, c); }
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { int16_t w = 1; if (rect(x, y, w, h)) t->writeFastVLine(x, y, h, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { int16_t w = 1; if (rect(x, y, w, h)) t->drawFastVLine(x, y, h, c); }
  void startWrite() override { t->startWrite(); }
  void endWrite() override { t->endWrite(); }
};
static ClipGfx clipGfx;

void uiClipBegin(int x0, int y0, int x1, int y1) {
  if (!uiClip.on) realG = G;
  uiClip = {true, (int16_t)(x0 < 0 ? 0 : x0), (int16_t)(y0 < 0 ? 0 : y0), (int16_t)(x1 > 240 ? 240 : x1), (int16_t)(y1 > 240 ? 240 : y1)};
  clipGfx.t = realG;
  clipGfx.setTextWrap(false);
  G = &clipGfx;
}

void uiClipEnd() {
  if (!uiClip.on) return;
  uiClip.on = false;
  G = realG;
}

// ---------- off-screen drawing (uiOffscreen) ----------
// No frame buffer (240x240 x 2 bytes doesn't fit in RAM): G becomes this strip of memory, which
// clips everything to (bx, by, bw, bh). Blits (vector fonts, images) land in it too.
class BufGfx : public Adafruit_GFX {
 public:
  uint16_t *buf = nullptr;
  int16_t bx = 0, by = 0, bw = 0, bh = 0;
  BufGfx() : Adafruit_GFX(240, 240) {}
  bool rect(int16_t &x, int16_t &y, int16_t &w, int16_t &h) {
    if (x < bx) { w -= bx - x; x = bx; }
    if (y < by) { h -= by - y; y = by; }
    if (x + w > bx + bw) w = bx + bw - x;
    if (y + h > by + bh) h = by + bh - y;
    return w > 0 && h > 0;
  }
  void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    if (!rect(x, y, w, h)) return;
    for (int r = 0; r < h; r++) {
      uint16_t *p = buf + (y - by + r) * bw + (x - bx);
      for (int i = 0; i < w; i++) p[i] = c;
    }
  }
  void put(int16_t x, int16_t y, const uint16_t *px, int16_t w, int16_t h) {
    for (int r = 0; r < h; r++) {
      int yy = y + r, x0 = x > bx ? x : bx, x1 = x + w < bx + bw ? x + w : bx + bw;
      if (yy >= by && yy < by + bh && x1 > x0) memcpy(buf + (yy - by) * bw + (x0 - bx), px + r * w + (x0 - x), (x1 - x0) * 2);
    }
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { fill(x, y, 1, 1, c); }
  void writePixel(int16_t x, int16_t y, uint16_t c) override { fill(x, y, 1, 1, c); }
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override { fill(x, y, w, h, c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override { fill(x, y, w, h, c); }
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fill(x, y, w, 1, c); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fill(x, y, w, 1, c); }
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fill(x, y, 1, h, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fill(x, y, 1, h, c); }
};
static BufGfx bufGfx;
static bool offscreen = false;

static void rawBlit(int16_t x, int16_t y, uint16_t *px, int16_t w, int16_t h) {
  if (offscreen) {
    for (int r = 0; r < h; r++) bufGfx.put(x, y + r, px + r * w, w, 1);
    return;
  }
  if (uiBlit) uiBlit(x, y, px, w, h);
  else (uiClip.on ? realG : G)->drawRGBBitmap(x, y, px, w, h);  // generic: pixel by pixel (simulator)
}

void blit(int16_t x, int16_t y, uint16_t *px, int16_t w, int16_t h) {
  if (!uiClip.on) {
    rawBlit(x, y, px, w, h);
    return;
  }
  for (int r = 0; r < h; r++) {  // clips row by row
    int yy = y + r;
    if (yy < uiClip.y0 || yy >= uiClip.y1) continue;
    int x0 = x > uiClip.x0 ? x : uiClip.x0, x1 = x + w < uiClip.x1 ? x + w : uiClip.x1;
    if (x1 > x0) rawBlit(x0, yy, px + r * w + (x0 - x), x1 - x0, 1);
  }
}

bool uiOffscreen(int x, int y, int w, int h, void (*draw)(void *), void *ctx) {
  if (offscreen) {  // already inside a strip (scroll change): the scene already clips to it
    draw(ctx);
    return true;
  }
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 240) w = 240 - x;
  if (y + h > 240) h = 240 - y;
  if (w <= 0 || h <= 0) return true;
  const int BYTES = 4096;  // weatherbox: ~4 KB per strip (fits even with fragmented memory); only exists during drawing
  int rows = BYTES / (2 * w);
  if (rows > h) rows = h;
  uint16_t *b = (uint16_t *)malloc(w * rows * 2);
  if (!b) return false;
  Adafruit_GFX *g0 = G;
  UiClip c0 = uiClip;
  for (int sy = y; sy < y + h; sy += rows) {
    int sh = y + h - sy < rows ? y + h - sy : rows;
    bufGfx.buf = b;
    bufGfx.bx = x;
    bufGfx.by = sy;
    bufGfx.bw = w;
    bufGfx.bh = sh;
    bufGfx.setTextWrap(false);
    uiClip = {true, (int16_t)x, (int16_t)sy, (int16_t)(x + w), (int16_t)(sy + sh)};  // blit() and images clip here
    offscreen = true;
    G = &bufGfx;
    draw(ctx);
    G = g0;
    offscreen = false;
    uiClip = c0;
    rawBlit(x, sy, b, w, sh);
    if (uiFrameHook) uiFrameHook();  // gives time to Wi-Fi between strips
  }
  free(b);
  return true;
}

void uiPaint(bool direct, int x, int y, int w, int h, void (*draw)(void *), void *ctx) {
  if (direct || !uiOffscreen(x, y, w, h, draw, ctx)) draw(ctx);
}

// =====================================================================================
// "Geo" vector font: digits as strokes (lines and arcs) on a 6x10 grid, in tenths.
// No bitmap in flash: works at any size and thickness (thin Neon, thick Flip).
// =====================================================================================
struct Stk { int8_t t; int16_t a, b, c, d, e; };  // t=0 line (a,b)-(c,d); t=1 arc: centre (a,b), radius c, from d to e degrees
static const Stk GEO0[] PROGMEM = {{1, 30, 30, 30, 180, 360}, {0, 0, 30, 0, 70, 0}, {0, 60, 30, 60, 70, 0}, {1, 30, 70, 30, 0, 180}};
static const Stk GEO1[] PROGMEM = {{0, 30, 0, 30, 100, 0}, {0, 8, 22, 30, 0, 0}};
static const Stk GEO2[] PROGMEM = {{1, 30, 30, 30, 180, 380}, {0, 58, 40, 0, 100, 0}, {0, 0, 100, 60, 100, 0}};
static const Stk GEO3[] PROGMEM = {{0, 0, 0, 60, 0, 0}, {0, 60, 0, 30, 40, 0}, {1, 30, 70, 30, 270, 510}};
static const Stk GEO4[] PROGMEM = {{0, 45, 100, 45, 0, 0}, {0, 45, 0, 0, 70, 0}, {0, 0, 70, 60, 70, 0}};
static const Stk GEO5[] PROGMEM = {{0, 60, 0, 5, 0, 0}, {0, 5, 0, 5, 48, 0}, {0, 5, 48, 2, 60, 0}, {1, 30, 70, 30, 200, 510}};
static const Stk GEO6[] PROGMEM = {{1, 30, 70, 30, 0, 360}, {0, 1, 62, 42, 0, 0}};
static const Stk GEO7[] PROGMEM = {{0, 0, 0, 60, 0, 0}, {0, 60, 0, 20, 100, 0}};
static const Stk GEO8[] PROGMEM = {{1, 30, 26, 26, 0, 360}, {1, 30, 70, 30, 0, 360}};
static const Stk GEO9[] PROGMEM = {{1, 30, 30, 30, 0, 360}, {0, 59, 38, 18, 100, 0}};
static const Stk *const GEO[10] PROGMEM = {GEO0, GEO1, GEO2, GEO3, GEO4, GEO5, GEO6, GEO7, GEO8, GEO9};
static const uint8_t GEO_N[10] PROGMEM = {4, 2, 3, 3, 3, 4, 2, 2, 2, 2};

// s = pixels per grid unit (the digit takes 6s x 10s); w = stroke thickness.
void geoDigit(float x, float y, int d, float s, float w, uint16_t c) {
  if (d < 0 || d > 9) return;
  const float k = s / 10.0f;
  const Stk *strokes = (const Stk *)pgm_read_ptr(&GEO[d]);
  for (int i = 0, n = pgm_read_byte(&GEO_N[d]); i < n; i++) {
    Stk st;
    memcpy_P(&st, &strokes[i], sizeof st);
    if (st.t == 0) {
      thickLine(x + st.a * k, y + st.b * k, x + st.c * k, y + st.d * k, w, c);
      continue;
    }
    float cx = x + st.a * k, cy = y + st.b * k, r = st.c * k;
    int steps = (st.e - st.d) / 12;
    if (steps < 6) steps = 6;
    float px = cx + r * cosf(st.d * K_PI / 180), py = cy + r * sinf(st.d * K_PI / 180);
    for (int j = 1; j <= steps; j++) {
      float a = (st.d + (st.e - st.d) * j / (float)steps) * K_PI / 180;
      float nx = cx + r * cosf(a), ny = cy + r * sinf(a);
      thickLine(px, py, nx, ny, w, c);
      px = nx;
      py = ny;
    }
  }
}

// Blends two RGB565 colours (f=0 -> a, f=1 -> b).
uint16_t mix(uint16_t a, uint16_t b, float f) {
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return ((int)(ar + (br - ar) * f) << 11) | ((int)(ag + (bg - ag) * f) << 5) | (int)(ab + (bb - ab) * f);
}

// "23° Partly cloudy" centred (degree sign drawn by hand)
void tempDesc(int cy, uint8_t sz, uint16_t ct, uint16_t cd, float t, const char *desc) {
  char n[8];
  snprintf(n, sizeof n, "%d", (int)lroundf(t));
  int degW = 2 * (sz + 1) + sz;
  int w = textW(n, sz) + degW + 6 * sz + textW(desc, sz);
  int x = 120 - w / 2;
  text(x, cy, sz, ct, n);
  degree(x + textW(n, sz) + sz, cy, sz, ct);
  text(x + textW(n, sz) + degW + 6 * sz, cy, sz, cd, desc);
}

// Table generated from faces_list.h: the theme number indexes the function, name and description.
typedef void (*FaceFn)(const UiState &, bool);
static const FaceFn FACE_FN[THEME_COUNT] PROGMEM = {
#define X(id, fn, name, desc) fn,
    UI_FACES(X)
#undef X
};
#define X(id, fn, name, desc) static const char FNAME_##id[] PROGMEM = name; static const char FDESC_##id[] PROGMEM = desc;
UI_FACES(X)
#undef X
static const char *const FACE_NAME[THEME_COUNT] PROGMEM = {
#define X(id, fn, name, desc) FNAME_##id,
    UI_FACES(X)
#undef X
};
static const char *const FACE_DESC[THEME_COUNT] PROGMEM = {
#define X(id, fn, name, desc) FDESC_##id,
    UI_FACES(X)
#undef X
};
PGM_P uiFaceName(uint8_t t) { return t < THEME_COUNT ? (PGM_P)pgm_read_ptr(&FACE_NAME[t]) : PSTR(""); }
PGM_P uiFaceDesc(uint8_t t) { return t < THEME_COUNT ? (PGM_P)pgm_read_ptr(&FACE_DESC[t]) : PSTR(""); }

// ---------- face change by hardware scrolling ----------
// 320-row ring: the old face (rows 0..239) moves up and out through the top, passes the black gap of the
// margin (80 rows) and the new one comes in from below, stopping at rows 0..239, where the offset returns to 0. Nothing
// is copied at the end. The new one is drawn in 16-row strips straight into the RAM rows the old one
// has already left, behind the scroll: strip n is only drawn after the old one has left the screen (offset
// >= 16*(n+1)) and must be ready before it comes in (offset <= 80 + 16*n). Each strip runs the
// whole face (full) clipped to it: costs ~15 full drawings, but only in drawing CPU;
// the scroll itself is one command per frame.
struct SlideCtx { const UiState *s; uint8_t t; };
static void slideStrip(void *p) {
  SlideCtx *c = (SlideCtx *)p;
  resetCaches();  // each strip is a full drawing from scratch
  G->fillScreen(BG);
  ((FaceFn)pgm_read_ptr(&FACE_FN[c->t]))(*c->s, true);
}
static uint32_t slideNow() {
  static uint32_t fake;  // no clock (simulator): 20 ms per query
  return uiMillis ? uiMillis() : (fake += 20);
}
static bool slideTo(const UiState &s, uint8_t t) {
  const int R = UI_PANEL_ROWS, STRIP = 16, N = 240 / STRIP, GAP = R - 240;
  const uint32_t MS = 700;
  if (!uiScrollBegin(R)) return false;
  SlideCtx c = {&s, t};
  uint32_t t0 = slideNow();
  int done = 0;  // strips drawn
  bool ok = true;
  for (;;) {
    uint32_t e = slideNow() - t0;
    uint32_t x = (e > MS ? MS : e) * 1024 / MS, sm = x * x * (3072 - 2 * x) >> 20;  // eases the start and end
    int want = R * sm / 1024, cap = done < N ? GAP + STRIP * done : R;
    int L = want < cap ? want : cap;
    uiScrollTo(L);  // L == R is offset 0 again, now with the new face
    if (L >= R) break;
    if (done < N && L >= STRIP * (done + 1)) {
      if (!uiOffscreen(0, STRIP * done, 240, STRIP, slideStrip, &c)) { ok = false; break; }  // no memory
      done++;
    } else if (uiFrameHook) uiFrameHook();
  }
  uiScrollEnd();
  return ok;  // false: the caller draws the new face the normal way (full screen)
}

void uiRenderClock(const UiState &s) {
  if (s.theme == THEME_PACK) {  // face installed as a pack (facepack.cpp)
    slideOn = false;
    bool full = enter(SCR_PACK);
    facePackRender(s, full);
    return;
  }
  uint8_t t = s.theme < THEME_COUNT ? s.theme : 0;
  if (slideOn) {
    slideOn = false;
    if (screen == -1 && slideTo(s, t)) {
      screen = t;
      return;
    }
  }
  bool full = enter(t);
  ((FaceFn)pgm_read_ptr(&FACE_FN[t]))(s, full);
}

// ---------- Pomodoro: progress ring ----------
static const TimeLayout TL_POMO = {{44, 79, 132, 167}, 70, 6, 117};

static uint16_t phaseColor(uint8_t p) { return p == PH_WORK ? WORK : p == PH_SHORT ? SHORT : LONG; }

static const int AX = 120, AY = 120;  // ring centre

static void ringDot(int deg, uint16_t c) {
  float a = (deg - 90) * K_PI / 180;
  G->fillCircle(AX + cosf(a) * 110, AY + sinf(a) * 110, 6, c);
}

void uiRenderPomodoro(const UiState &s) {
  bool full = enter(SCR_POMO);
  uint16_t col = phaseColor(s.phase);
  // ring: unlit track + progress in the phase colour, drawn only on the new stretch
  int target = s.totalMs ? (int)((uint64_t)(s.totalMs - s.remainingMs) * 360 / s.totalMs) : 0;
  if (full || target < lastRing) {
    for (int d = 0; d < 360; d += 1) ringDot(d, DIM);
    lastRing = 0;
    ringDot(0, col);
  }
  for (int d = lastRing + 1; d <= target; d++) ringDot(d, col);
  if (target > lastRing) lastRing = target;

  uint8_t act = s.activity < ACT_COUNT ? s.activity : ACT_READ;
  const char *label = s.phase == PH_WORK ? activityLabel(act) : phaseLabel(s.phase);
  if (changed(cA, label)) {
    G->fillRect(50, 44, 140, 18, BG);  // inside the ring (inner radius 104)
    textC(120, 46, 2, col, label);
  }

  uint32_t secs = (s.remainingMs + 999) / 1000;
  int m = secs / 60 > 99 ? 99 : secs / 60, sec = secs % 60;
  // paused: the digits blink slowly
  bool show = s.running || (s.timeOk ? s.now.tm_sec % 2 == 0 : true);
  if (show) drawTime4(TL_POMO, m / 10, m % 10, sec / 10, sec % 10, true, col, BG);
  else drawTime4(TL_POMO, -1, -1, -1, -1, false, col, BG);

  int total = s.longEvery ? s.longEvery : 1;
  int done = s.phase == PH_LONG ? total : s.cycleDone % total;
  if (done != lastDots || total != lastGoal) {
    G->fillRect(40, 122, 160, 14, BG);
    int gap = 16, x0 = 120 - (total - 1) * gap / 2;
    for (int i = 0; i < total; i++) {
      if (i < done) G->fillCircle(x0 + i * gap, 129, 4, WORK);
      else G->drawCircle(x0 + i * gap, 129, 4, MUTED);
    }
    lastDots = done;
    lastGoal = total;
  }

  // tomato + the activity's object (focus) or coffee (break); when a phase ends, the end animation
  int8_t key;  // face * 8 + object (0..ACT_COUNT-1 = activity, 7 = coffee); 64+ = animation frame
  if (s.celebrate) key = 64 + uiSpriteFrame(&SPR_TOMATO_DONE, s.ms);
  else key = (!s.running ? FACE_SLEEP : s.phase == PH_WORK ? FACE_FOCUS : FACE_RELAX) * 8 + (s.phase == PH_WORK ? act : 7);
  if (key != lastFace) {
    if ((key >= 64) != (lastFace >= 64)) G->fillRect(84, 136, 72, 44, BG);  // layout change
    if (key >= 64) uiSprite(&SPR_TOMATO_DONE, 104, 138, 2, key - 64, BG);    // bg repaints the previous frame
    else {
      mascot(86, 144, 2, key / 8, BG);
      uiSprite(key % 8 == 7 ? &SPR_COFFEE : (const UiSprite *)pgm_read_ptr(&ACTIVITY_SPRITE[key % 8]), 122, 144, 2, 0, BG);
    }
    lastFace = key;
  }

  char st[24];
  if (!s.running) snprintf(st, sizeof st, "%s", tr("PAUSADO", "PAUSED", "PAUSADO"));
  else if (s.timeOk) {
    struct tm t = s.now, e;
    time_t end = mktime(&t) + secs;
    localtime_r(&end, &e);
    snprintf(st, sizeof st, tr("termina %02d:%02d", "ends %02d:%02d", "termina %02d:%02d"), e.tm_hour, e.tm_min);
  } else st[0] = 0;
  if (changed(cB, st)) {
    uiTextBox(50, 184, 140, 10, BG, 120, 186, 1, s.running ? MUTED : TEXT, st, 'c');
  }
}

// ---------- one-off screens ----------
static int8_t bootLast = -1;  // drawn position of the start-up bar (-1: new screen, draw the frame)
struct MsgJob { const char *title, *l1, *l2, *l3; };
static void paintMessage(void *p) {
  const MsgJob &m = *(const MsgJob *)p;
  const char *title = m.title, *l1 = m.l1, *l2 = m.l2, *l3 = m.l3;
  G->fillRect(0, 0, 240, 190, BG);
  // weatherbox: sun-and-cloud logo and the title in the Outfit font, in place of the tomato
  weatherIcon(120, 50, 1.45f, 2, true, BG);
  const int tw = vecWidth(VF_OUTFIT_TXT, title, 28);
  vecText(VF_OUTFIT_TXT, 120 - tw / 2, 88, 28, title, TEXT, BG);
  textC(120, 126, 2, LONG, l1);
  textC(120, 158, 1, MUTED, l2);
  textC(120, 172, 1, MUTED, l3);
}
void uiMessage(const char *title, const char *l1, const char *l2, const char *l3) {
  uiScrollEnd();
  const bool again = screen == SCR_MSG;
  screen = SCR_MSG;
  // weatherbox: the whole screen is built off-screen and sent finished (no black flash between messages);
  // the start-up bar, if it was already on screen, is redrawn by uiBootProgress
  MsgJob m = {title, l1, l2, l3};
  if (!again) {  // new screen: also clear the bar's strip
    G->fillRect(0, 190, 240, 50, BG);
    bootLast = -1;
  }
  uiPaint(false, 0, 0, 240, 190, paintMessage, &m);  // the bar (y >= 192) stays where it is
}

// Changes only the highlighted line of the message on screen (e.g. "Starting" -> "Connecting Wi-Fi"), without redrawing the rest
void uiMessageLine(const char *l1) {
  if (screen != SCR_MSG) return;
  uiTextBox(0, 124, 240, 20, BG, 120, 126, 2, LONG, l1, 'c');
}

// Start-up screen: a pixel-art bar (frame with cut corners and 16 blocks of 8x8). While a step is still
// missing (Wi-Fi, time), 4 lit blocks bounce back and forth, block by block (there's no way to know how much
// is left); done >= 2: they all light up. Only redraws when the position changes, without redoing the screen.
void uiBootProgress(uint32_t ms, uint8_t done) {
  if (screen != SCR_MSG) return;
  const int N = 16, B = 8, GAP = 2, LIT = 4, w = N * (B + GAP) - GAP + 8, h = B + 8, x0 = 120 - w / 2, y0 = 192;
  int pos = done >= 2 ? N : (int)(ms / 90 % (2 * (N - LIT)));
  if (pos < N && pos > N - LIT) pos = 2 * (N - LIT) - pos;
  if (pos == bootLast) return;
  if (bootLast < 0) {  // frame: 2 px, without the corners
    G->fillRect(x0 + 2, y0, w - 4, 2, MUTED);
    G->fillRect(x0 + 2, y0 + h - 2, w - 4, 2, MUTED);
    G->fillRect(x0, y0 + 2, 2, h - 4, MUTED);
    G->fillRect(x0 + w - 2, y0 + 2, 2, h - 4, MUTED);
  }
  bootLast = pos;
  for (int i = 0; i < N; i++) {
    bool on = pos == N || (i >= pos && i < pos + LIT);
    G->fillRect(x0 + 4 + i * (B + GAP), y0 + 4, B, B, on ? LONG : CARD2);
  }
}

// weatherbox: small clock in the bottom-right corner, in a dark box (on top of the album's radar)
void uiCornerClock(const char *hm) {
  const int w = strlen(hm) * 12 - 2, x0 = 240 - w - 8, y0 = 216;
  // off-screen, as a whole rectangle (the rounded corners stay black): changes without flicker
  uiPaint(false, x0 - 5, y0 - 4, w + 10, 24, [](void *p) {
    const char *hm = (const char *)p;
    const int w = strlen(hm) * 12 - 2, x0 = 240 - w - 8, y0 = 216;
    G->fillRect(x0 - 5, y0 - 4, w + 10, 24, 0);
    text(x0, y0, 2, TEXT, hm);
  }, (void *)hm);
}

// ---------- weatherbox: first install ----------
#include "src/qrcode/qrcode.h"
// QR code with a 2-module white margin, centred on cx starting at y; returns the height used
static int drawQr(int cx, int y, const char *data, int maxPx) {
  QRCode qr;
  uint8_t buf[300];
  int v = 2;
  for (; v <= 7; v++)
    if (qrcode_getBufferSize(v) <= sizeof buf && qrcode_initText(&qr, buf, v, ECC_LOW, data) == 0) break;
  if (v > 7) return 0;
  const int n = qr.size + 4, px = maxPx / n > 1 ? maxPx / n : 1, side = n * px, x0 = cx - side / 2;
  G->fillRoundRect(x0 - 2, y - 2, side + 4, side + 4, 6, uiRgb(255, 255, 255));
  for (int my = 0; my < qr.size; my++)
    for (int mx = 0; mx < qr.size; mx++)
      if (qrcode_getModule(&qr, mx, my)) G->fillRect(x0 + (mx + 2) * px, y + (my + 2) * px, px, px, 0);
  return side;
}

static void setupTitle(const char *t) {
  const int w = vecWidth(VF_OUTFIT_TXT, t, 24);
  vecText(VF_OUTFIT_TXT, 120 - w / 2, 4, 24, t, TEXT, BG);
}

void uiSetupWifi(const char *ssid, const char *pass) {
  uiScrollEnd();
  screen = SCR_MSG;
  G->fillScreen(BG);
  setupTitle(tr("Configurar o Wi-Fi", "Set up Wi-Fi", "Configurar el Wi-Fi"));
  char q[96];
  snprintf(q, sizeof q, "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
  const int h = drawQr(120, 32, q, 104);
  int y = 32 + h + 6;
  textC(120, y, 1, LONG, tr("1. Aponte a camera do celular", "1. Scan with your phone's camera", "1. Escanea con la camara del movil"));
  y += 12;
  char l[48];
  snprintf(l, sizeof l, tr("ou entre em %s", "or join %s", "o entra en %s"), ssid);
  textC(120, y, 1, MUTED, l);
  y += 10;
  snprintf(l, sizeof l, tr("senha: %s", "password: %s", "clave: %s"), pass);
  textC(120, y, 1, TEXT, l);
  y += 14;
  textC(120, y, 1, LONG, tr("2. Escolha a sua rede Wi-Fi", "2. Pick your home Wi-Fi", "2. Elige tu red Wi-Fi"));
  y += 10;
  textC(120, y, 1, MUTED, tr("(abre sozinho ou va a 192.168.4.1)", "(opens by itself, or go to 192.168.4.1)", "(se abre solo o ve a 192.168.4.1)"));
  y += 14;
  textC(120, y, 1, SUN, tr("So Wi-Fi de 2,4 GHz (nao 5 GHz)", "2.4 GHz Wi-Fi only (not 5 GHz)", "Solo Wi-Fi de 2,4 GHz (no 5 GHz)"));
}

void uiSetupCity(const char *ip, const char *host) {
  uiScrollEnd();
  screen = SCR_MSG;
  G->fillScreen(BG);
  setupTitle(tr("Quase pronto!", "Almost done!", "Casi listo!"));
  char u[48];
  snprintf(u, sizeof u, "http://%s/", ip);
  const int h = drawQr(120, 34, u, 112);
  int y = 34 + h + 8;
  textC(120, y, 1, LONG, tr("Aponte a camera para escolher", "Scan with your phone to choose", "Escanea con el movil para elegir"));
  y += 10;
  textC(120, y, 1, LONG, tr("a sua cidade", "your city and see the weather", "tu ciudad y ver el clima"));
  y += 14;
  snprintf(u, sizeof u, tr("ou abra %s", "or open %s", "o abre %s"), ip);
  textC(120, y, 1, TEXT, u);
  y += 10;
  snprintf(u, sizeof u, "(%s.local)", host);
  textC(120, y, 1, MUTED, u);
}

// While the clock has no time: four little dots at the top, one lit that moves back and forth, over the face
void uiSyncHint(uint32_t ms) {
  int p = ms / 200 % 6, on = p < 4 ? p : 6 - p;
  for (int i = 0; i < 4; i++) G->fillCircle(108 + i * 8, 5, 2, i == on ? LONG : MUTED);
}

void uiColorTest() {
  uiScrollEnd();
  screen = SCR_COLOR;
  G->fillScreen(BG);
  textC(120, 8, 1, TEXT, tr("TESTE DE CORES", "COLOR TEST", "PRUEBA DE COLOR"));
  const uint16_t c[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  const char *n[4] = {tr("VERMELHO", "RED", "ROJO"), tr("VERDE", "GREEN", "VERDE"), tr("AZUL", "BLUE", "AZUL"), tr("BRANCO", "WHITE", "BLANCO")};
  for (int i = 0; i < 4; i++) {
    uiTextBox(i * 60 + 4, 26, 52, 160, c[i], i * 60 + 30, 194, 1, TEXT, n[i], 'c');
  }
  textC(120, 218, 1, MUTED, tr("o 1o deve ser vermelho e o", "the 1st should be red and the", "la 1a debe ser roja y el"));
  textC(120, 228, 1, MUTED, tr("fundo preto", "background black", "fondo negro"));
}

// Anti-ghosting test: mid grey alternating with black pixel by pixel. With the VCOM (the panel's common
// voltage) off the sweet spot, the "positive" and "negative" pixels of each frame end up with different
// brightness and the pattern shimmers; at the right point it stays still, and ghosting is at its lowest.
// Four patterns because the panel can invert polarity per dot, per row or per column.
void uiVcomTest(uint8_t vcom, bool full) {
  static const uint16_t GRAY = uiRgb(128, 128, 128);
  if (full) {
    uiScrollEnd();
    screen = SCR_VCOM;
    G->fillScreen(BG);
    textC(120, 6, 1, TEXT, tr("ANTI-FANTASMA", "ANTI-GHOSTING", "ANTIFANTASMA"));
    for (int q = 0; q < 4; q++) {
      const int x0 = 8 + (q % 2) * 116, y0 = 40 + (q / 2) * 92;
      for (int y = 0; y < 84; y++)
        for (int x = 0; x < 108; x++) {
          bool on = q == 0 ? ((x + y) & 1) : q == 1 ? (y & 1) : q == 2 ? (x & 1) : true;
          if (on) G->drawPixel(x0 + x, y0 + y, GRAY);
        }
    }
    textC(120, 218, 1, MUTED, tr("mova ate a estampa parar", "move until the pattern stops", "mueve hasta que no tiemble"));
    textC(120, 228, 1, MUTED, tr("de tremer", "flickering", "el patron"));
  }
  int mv = 100 + 25 * vcom;  // ST7789 VCOMS register: 0.100 V + 25 mV per step
  char v[20];
  snprintf(v, sizeof v, "VCOM %d%c%03d V", mv / 1000, uiLang == 1 ? '.' : ',', mv % 1000);
  uiTextBox(0, 18, 240, 18, BG, 120, 20, 2, vcom == UI_VCOM_DEFAULT ? TEXT : SUN, v, 'c');
}

void uiError(const char *msg) {
  uiScrollEnd();
  screen = SCR_ERROR;
  G->fillScreen(BG);
  textC(120, 112, 2, MUTED, msg);
}
