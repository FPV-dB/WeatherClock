// Drone fly check face (weatherbox): is it safe to fly the drone chosen in the panel?
// Same rules as WindBar2 (github.com/FPV-dB/windbar2): sustained wind and gusts are each compared with the
// drone's limits; under 70% = good, under 90% = caution, up to 100% = warning, over = don't fly. The worse of
// the two wins. Rain on top makes it at least a warning. Everything comes from the clock's own weather:
// no computer program is needed.
// Registered in faces_list.h; uses the drawing kit in ui_kit.h.
#include "ui_kit.h"

enum Cond : uint8_t { C_GOOD, C_CAUTION, C_WARNING, C_ALERT };
static const uint16_t ORANGE = uiRgb(255, 140, 40);

static uint16_t condColor(uint8_t c) {
  return c == C_GOOD ? SHORT : c == C_CAUTION ? SUN : c == C_WARNING ? ORANGE : WORK;
}
static const char *condLabel(uint8_t c) {
  return c == C_GOOD ? "GOOD TO FLY" : c == C_CAUTION ? "CAUTION" : c == C_WARNING ? "WARNING" : "DON'T FLY";
}
// ratio in percent of the limit
static uint8_t condFor(int pct) { return pct < 70 ? C_GOOD : pct < 90 ? C_CAUTION : pct <= 100 ? C_WARNING : C_ALERT; }

// WindBar2's aircraft profiles (km/h): name, sustained wind, gusts
struct Profile { const char *name; uint8_t wind, gust; };
static Profile profileOf(const UiState &s) {
  static const Profile P[DRONE_CUSTOM] = {
      {"DJI Neo 1", 25, 25}, {"DJI Neo 2", 20, 24}, {"BetaFPV Pavo 20 Pro", 36, 36}, {"DJI Avata 2", 30, 40}, {"65mm Tinywhoop", 15, 20}};
  const UiDrone *d = s.drone;
  if (!d || d->id >= DRONE_COUNT) return P[DRONE_NEO2];
  if (d->id < DRONE_CUSTOM) return P[d->id];
  Profile c = {d->name[0] ? d->name : "Custom drone", d->wind ? d->wind : (uint8_t)20, d->gust ? d->gust : (uint8_t)25};
  return c;
}

static bool rainCode(int code) { return (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95; }

struct Verdict { uint8_t cond, windPct, gustPct; bool rain; };
static Verdict verdict(const UiState &s, const Profile &p) {
  Verdict v;
  v.windPct = (uint8_t)(s.wx.wind * 100 / p.wind > 250 ? 250 : s.wx.wind * 100 / p.wind);
  v.gustPct = (uint8_t)(s.wx.gust * 100 / p.gust > 250 ? 250 : s.wx.gust * 100 / p.gust);
  const uint8_t a = condFor(v.windPct), b = condFor(v.gustPct);
  v.cond = a > b ? a : b;
  v.rain = rainCode(s.wx.code);
  if (v.rain && v.cond < C_WARNING) v.cond = C_WARNING;
  return v;
}

static uint8_t hourCond(const UiState &s, const Profile &p, int i) {
  const uint8_t a = condFor(s.wx.hw[i] * 100 / p.wind), b = condFor(s.wx.hg[i] * 100 / p.gust);
  uint8_t c = a > b ? a : b;
  if (s.wx.hmm[i] >= 2 && c < C_WARNING) c = C_WARNING;  // 0.2 mm of rain or more in that hour
  return c;
}

// ---------- the animated drone (top area, y 24..105) ----------
static const int AX = 0, AY = 24, AW = 240, AH = 82;

static void rot(float cx, float cy, float a, float x, float y, float &ox, float &oy) {
  const float c = cosf(a), s = sinf(a);
  ox = cx + x * c - y * s;
  oy = cy + x * s + y * c;
}

static void paintSky(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  G->fillRect(AX, AY, AW, AH, BG);
  const Profile p = profileOf(s);
  const Verdict v = verdict(s, p);
  const float t = s.ms / 1000.0f;
  // horizontal part of where the wind blows to (+1 = to the right/east)
  const float to = (s.wx.wdir + 180) * K_PI / 180;
  float dx = sinf(to);
  if (fabsf(dx) < 0.25f) dx = dx < 0 ? -0.25f : 0.25f;
  // wind streaks across the area, faster and longer with more wind
  const int n = s.wx.wind < 3 ? 0 : s.wx.wind < 12 ? 7 : 12;
  const float speed = (20 + s.wx.wind * 2.2f) * dx;
  const int len = 6 + (s.wx.wind > 40 ? 20 : s.wx.wind / 2);
  for (int i = 0; i < n; i++) {
    const int y = AY + 28 + (i * 17) % (AH - 32);  // below the corner texts
    const float ph = (i * 0.61803f - (int)(i * 0.61803f)) * (AW + 60);
    float x = fmodf(ph + t * speed * (0.8f + (i % 3) * 0.15f), AW + 60);
    if (x < 0) x += AW + 60;
    x -= 30;
    const float edge = 1 - fabsf(x - AW / 2) / (AW / 2 + 30);
    const uint16_t c = mix(BG, LONG, 0.25f + 0.45f * (edge < 0 ? 0 : edge));
    G->drawFastHLine(dx > 0 ? x - len : x, y, len, c);
  }
  // the drone: bobs up and down, leans into the wind, wobbles with the gusts
  const float bob = sinf(t * 2.1f) * 3;
  float gustAmp = (s.wx.gust > s.wx.wind ? (s.wx.gust - s.wx.wind) * 0.006f : 0) + (v.cond == C_ALERT ? 0.06f : 0.01f);
  if (gustAmp > 0.12f) gustAmp = 0.12f;
  float lean = -dx * (v.windPct > 120 ? 120 : v.windPct) / 100.0f * 0.30f;  // nose into the wind
  lean += sinf(t * 2.7f) * gustAmp + sinf(t * 6.3f) * gustAmp * 0.4f;
  const float cx = 120 + (v.cond == C_ALERT ? sinf(t * 1.3f) * 10 * dx : 0), cy = AY + 46 + bob;
  float x0, y0, x1, y1;
  // arms
  rot(cx, cy, lean, -44, 0, x0, y0);
  rot(cx, cy, lean, 44, 0, x1, y1);
  thickLine(x0, y0, x1, y1, 3, CARD2);
  // motors and propellers (a blade whose length goes in and out, over a faint disc)
  for (int side = -1; side <= 1; side += 2) {
    float mx, my, px, py;
    rot(cx, cy, lean, side * 44, 0, mx, my);
    G->fillRoundRect(mx - 4, my - 6, 9, 8, 2, MUTED);
    rot(cx, cy, lean, side * 44, -9, px, py);
    const float r = 22;
    float ax, ay, bx, by;
    rot(px, py, lean, -r, 0, ax, ay);
    rot(px, py, lean, r, 0, bx, by);
    G->drawLine(ax, ay, bx, by, DIM);
    const float k = cosf(t * 31.0f + side);  // spin
    rot(px, py, lean, -r * k, 0, ax, ay);
    rot(px, py, lean, r * k, 0, bx, by);
    thickLine(ax, ay, bx, by, 2, CLOUD);
    G->drawLine(mx, my - 6, px, py, MUTED);
  }
  // body with the camera and the status light (blinks in the verdict's colour)
  float bx, by;
  rot(cx, cy, lean, 0, 1, bx, by);
  G->fillRoundRect(bx - 17, by - 8, 34, 16, 6, CLOUD);
  G->fillRoundRect(bx - 13, by - 5, 26, 10, 4, DCLOUD);
  float lx, ly;
  rot(cx, cy, lean, dx > 0 ? 10 : -10, 1, lx, ly);
  G->fillCircle(lx, ly, 3, BG);
  G->fillCircle(lx, ly, 1, MUTED);
  const bool on = ((s.ms / 250) % (v.cond == C_GOOD ? 6 : 2)) == 0;
  rot(cx, cy, lean, dx > 0 ? -9 : 9, -1, lx, ly);
  G->fillCircle(lx, ly, 2, on ? condColor(v.cond) : mix(BG, condColor(v.cond), 0.35f));
  // corners: where it blows from, temperature and UV
  char b[24];
  snprintf(b, sizeof b, "FROM %s", windDir(s.wx.wdir));
  text(8, AY + 4, 1, MUTED, b);
  if (s.wx.uv10 != 255) {
    snprintf(b, sizeof b, "UV %u", (unsigned)((s.wx.uv10 + 5) / 10));
    textR(232, AY + 16, 1, MUTED, b);
  }
  snprintf(b, sizeof b, "%d", (int)lroundf(s.wx.t));
  const int w = textW(b, 1);
  text(226 - w, AY + 4, 1, MUTED, b);
  degree(227, AY + 4, 1, MUTED);
  if (v.rain) text(8, AY + 16, 1, RAIN, "RAIN");
}

// ---------- verdict, limits and wind cards ----------
static void meter(int x, const char *label, uint8_t kmh, uint8_t limit, uint8_t pct) {
  const int y = 162, w = 108, h = 42;
  G->fillRoundRect(x, y, w, h, 8, CARD);
  text(x + 8, y + 5, 1, MUTED, label);
  char b[8];
  snprintf(b, sizeof b, "%u", kmh);
  const int vw = vecWidth(VF_OUTFIT, b, 18);
  vecText(VF_OUTFIT, x + 8, y + 14, 18, b, TEXT, CARD);
  text(x + 12 + vw, y + 23, 1, MUTED, "km/h");
  snprintf(b, sizeof b, "/%u", limit);
  textR(x + w - 8, y + 5, 1, MUTED, b);
  // bar: full width = 125% of the limit, white tick at the limit
  const int bx = x + 8, bw = w - 16, by = y + h - 7;
  G->fillRoundRect(bx, by, bw, 4, 2, CARD2);
  int fw = (int)(pct > 125 ? 125 : pct) * bw / 125;
  if (fw > 0) G->fillRoundRect(bx, by, fw < 4 ? 4 : fw, 4, 2, condColor(condFor(pct)));
  G->drawFastVLine(bx + bw * 100 / 125, by - 2, 8, TEXT);
}

static void paintMiddle(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  G->fillRect(0, 106, 240, 100, BG);
  const Profile p = profileOf(s);
  const Verdict v = verdict(s, p);
  const uint16_t c = condColor(v.cond);
  G->fillRoundRect(14, 107, 212, 30, 15, mix(BG, c, 0.22f));
  G->drawRoundRect(14, 107, 212, 30, 15, c);
  textC(120, 111, 3, c, condLabel(v.cond));
  char b[48];
  snprintf(b, sizeof b, "%s", p.name);
  textC(120, 141, 1, TEXT, b);
  const char *why;
  if (v.rain) why = "Raining: keep it on the ground";
  else if (v.cond == C_GOOD) why = "Wind and gusts well within limits";
  else {
    const char *src = v.gustPct >= v.windPct ? "Gusts" : "Wind";
    snprintf(b, sizeof b, "%s %s limits", src, v.cond == C_CAUTION ? "near" : v.cond == C_WARNING ? "close to" : "over");
    why = b;
  }
  textC(120, 151, 1, MUTED, why);
  meter(8, "WIND", s.wx.wind, p.wind, v.windPct);
  meter(124, "GUSTS", s.wx.gust, p.gust, v.gustPct);
}

// ---------- the next 12 hours ----------
static void paintHours(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  G->fillRect(0, 206, 240, 34, BG);
  const Profile p = profileOf(s);
  const int n = s.wx.nh > 12 ? 12 : s.wx.nh;
  text(8, 208, 1, MUTED, "NEXT 12 H");
  if (!n) return;
  // best window: the longest run of good hours (the first one if tied)
  int best = -1, bestLen = 0;
  for (int i = 0; i < n;) {
    if (hourCond(s, p, i) != C_GOOD) { i++; continue; }
    int j = i;
    while (j < n && hourCond(s, p, j) == C_GOOD) j++;
    if (j - i > bestLen) { best = i; bestLen = j - i; }
    i = j;
  }
  char b[24];
  if (best < 0) textR(232, 208, 1, WORK, "NO GOOD HOURS");
  else {
    const int h0 = (s.wx.h0 + best) % 24, h1 = (s.wx.h0 + best + bestLen) % 24;
    if (best == 0 && bestLen == n) snprintf(b, sizeof b, "GOOD ALL DAY");
    else snprintf(b, sizeof b, "BEST %02d:00-%02d:00", h0, h1);
    textR(232, 208, 1, SHORT, b);
  }
  for (int i = 0; i < n; i++) {
    const int x = 8 + i * 224 / 12, w = 224 / 12 - 2;
    G->fillRoundRect(x, 219, w, 9, 2, condColor(hourCond(s, p, i)));
    if (i % 3 == 0) {
      snprintf(b, sizeof b, "%02d", (s.wx.h0 + i) % 24);
      text(x, 231, 1, MUTED, b);
    }
  }
}

static void paintAll(void *ctx) {
  paintSky(ctx);
  paintMiddle(ctx);
  paintHours(ctx);
}

void themeDrone(const UiState &s, bool full) {
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
  char l[24];
  dateLabel(s, l, sizeof l);
  if (changed(cStatL, l)) uiTextBox(0, 4, 140, 10, BG, 10, 7, 1, MUTED, l, 'l');
  if (changed(cC, hm)) uiTextBox(150, 0, 90, 22, BG, 230, 4, 2, s.accent, hm, 'r');
  const bool ok = s.weatherOn && s.wx.ok;
  // redraw everything when the weather or the drone changes
  char key[40];
  const UiDrone *d = s.drone;
  snprintf(key, sizeof key, "%d %u %u %u %u %s", ok, s.wx.version, d ? d->id : 255, d ? d->wind : 0, d ? d->gust : 0,
           d ? d->name : "");
  static uint32_t lastAnim = 0;
  if (full || changed(cA, key)) {
    lastAnim = s.ms;
    if (!ok) {
      G->fillRect(0, 24, 240, 216, BG);
      textC(120, 110, 1, MUTED, "set the city in the panel");
      return;
    }
    uiPaint(full, 0, 24, 240, 216, paintAll, (void *)&s);
    return;
  }
  // only the drone and the wind move: ~8 frames per second, drawn off-screen (no flicker)
  if (ok && s.ms - lastAnim >= 120) {
    lastAnim = s.ms;
    uiPaint(false, AX, AY, AW, AH, paintSky, (void *)&s);
  }
}
