// Lightning face: live strikes near the clock (nearest strike, how many in the last 10 minutes, where the storm
// is and where it is heading). The computer program follows a live lightning feed and posts to /api/lightning.
// Three states: lightning detected (flashes), all clear (no strike within the radius for 30 minutes) and
// waiting (no data from the computer in the last 5 minutes).
#include "ui_kit.h"

static const uint16_t BOLT = uiRgb(255, 214, 60);
static const uint16_t AMBER = uiRgb(255, 150, 40);
static const uint16_t CLEAR = uiRgb(111, 219, 172);
static const uint16_t FLASH_BG = uiRgb(28, 26, 40);
static const uint16_t FLASH_CARD = uiRgb(46, 44, 60);
static const uint32_t NO_STRIKE = 0xFFFFFFFFUL;

enum LtMode : uint8_t { LT_WAIT, LT_CLEAR, LT_ACTIVE };

struct LtView {
  const UiState *s;
  uint8_t mode;
  bool flash;
  uint32_t ago;  // seconds since the latest strike, now (NO_STRIKE = none known)
};

static uint32_t ltSince(const UiState &s) {  // ms since the data arrived
  const int32_t d = (int32_t)(s.ms - s.lightning->at);
  return d < 0 ? 0 : (uint32_t)d;
}

static uint32_t ltAgo(const UiState &s) {
  const UiLightning *l = s.lightning;
  if (!l || !l->at || l->ago == NO_STRIKE) return NO_STRIKE;
  return l->ago + ltSince(s) / 1000;
}

static uint8_t ltMode(const UiState &s) {
  const UiLightning *l = s.lightning;
  if (!l || !l->at || ltSince(s) > 5UL * 60000UL) return LT_WAIT;
  const uint32_t ago = ltAgo(s);
  return ago != NO_STRIKE && ago < 30UL * 60 && l->nearKm >= 0 ? LT_ACTIVE : LT_CLEAR;
}

// Double flash and a single one, every 4 seconds, while strikes are coming in.
static bool ltFlash(const UiState &s, uint8_t mode) {
  if (mode != LT_ACTIVE || !s.lightning->n10) return false;
  const uint32_t f = s.ms % 4000;
  return (f >= 300 && f < 420) || (f >= 520 && f < 640) || (f >= 1400 && f < 1520);
}

// Number in Outfit with an optional unit in the pixel font, centred on cx and no wider than maxW (it shrinks,
// keeping its baseline).
static void ltNum(int cx, int y, int h, const char *v, uint16_t c, uint16_t bg, const char *unit, int maxW, int minX = 0) {
  const VecFont &f = strchr(v, '-') ? VF_OUTFIT_TXT : VF_OUTFIT;
  const int uw = unit ? textW(unit, 2) + 4 : 0;
  const int h0 = h;
  int dw = vecWidth(f, v, h);
  while (dw + uw > maxW && h > 14) dw = vecWidth(f, v, h -= 2);
  y += h0 - h;
  int x = cx - (dw + uw) / 2;
  if (x < minX) x = minX;
  vecText(f, x, y, h, v, c, bg);
  if (unit) text(x + dw + 4, y + h - 16, 2, c, unit);
}

static const char *compass8(int deg) {
  static const char D[8][3] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return D[((deg % 360 + 360) % 360 * 2 + 45) / 90 % 8];
}

static uint16_t ltTitleColor(const UiState &s) {
  int p = 8 + (int)(8 * sinf(s.ms * 0.0052f));
  return mix16(AMBER, BOLT, p < 0 ? 0 : p > 16 ? 16 : p);
}

static void ltTitle(const LtView &v, uint16_t bg) {
  if (v.mode == LT_ACTIVE) textC(120, 124, 2, ltTitleColor(*v.s), "Lightning detected");
  else if (v.mode == LT_CLEAR) textC(120, 124, 2, CLEAR, "No lightning nearby");
  else textC(120, 124, 2, MUTED, "Lightning");
  (void)bg;
}

static void ltIcon(const LtView &v, uint16_t bg) {
  const UiState &s = *v.s;
  if (v.mode == LT_ACTIVE) weatherIcon(64, 82, 1.6f, 95, true, bg, s.ms);
  else if (s.weatherOn && s.wx.ok) weatherIcon(64, 82, 1.6f, s.wx.code, s.wx.day, bg, s.ms);
  else weatherIcon(64, 82, 1.6f, 3, true, bg, s.ms);
}

// "<city> - storm approaching", shortening the city so the line fits the screen (39 characters).
static void ltSubtitle(const LtView &v, char *out, size_t n) {
  const UiState &s = *v.s;
  const UiLightning *l = s.lightning;
  char what[40];
  if (v.mode == LT_WAIT) {
    snprintf(out, n, "needs the radar program on a computer");
    return;
  }
  if (v.mode == LT_CLEAR) snprintf(what, sizeof what, "watching %u km around", l->radius);
  else if (v.ago >= 600) snprintf(what, sizeof what, "last strike %lu min ago", (unsigned long)(v.ago / 60));
  else if (l->moving >= 0 && l->from >= 0) {
    int d = abs(((l->moving - (l->from + 180)) % 360 + 540) % 360 - 180);  // 0 = heading straight for us
    snprintf(what, sizeof what, "%s", d <= 60 ? "storm approaching" : d >= 120 ? "storm moving away" : "thunderstorm nearby");
  } else snprintf(what, sizeof what, "thunderstorm nearby");
  const char *city = s.city && s.city[0] ? s.city : "";
  int room = 39 - (int)strlen(what) - 3;
  if (!city[0] || room < 4) snprintf(out, n, "%s", what);
  else snprintf(out, n, "%.*s - %s", room, city, what);
}

static void ltCard(int k, const char *label, uint16_t card) {
  const int x = 8 + k * 77;
  G->fillRoundRect(x, 160, 70, 72, 12, card);
  textC(x + 35, 172, 1, MUTED, label);
}

static void paintLightning(void *ctx) {
  const LtView &v = *(const LtView *)ctx;
  const UiState &s = *v.s;
  const UiLightning *l = s.lightning;
  const uint16_t bg = v.flash ? FLASH_BG : BG;
  const uint16_t card = v.flash ? FLASH_CARD : CARD;
  G->fillRect(0, 0, 240, 240, bg);
  char t[48];
  dateLabel(s, t, sizeof t);
  text(10, 7, 1, MUTED, t);
  if (s.timeOk) snprintf(t, sizeof t, "%02d:%02d", displayHour(s), s.now.tm_min);
  else snprintf(t, sizeof t, "--:--");
  vecText(VF_OUTFIT, 230 - vecWidth(VF_OUTFIT, t, 18), 3, 18, t, s.accent, bg);
  ltIcon(v, bg);

  // big number: nearest strike, or the distance watched when all is clear
  if (v.mode == LT_ACTIVE) {
    snprintf(t, sizeof t, "%d", l->nearKm);
    ltNum(166, 46, 58, t, TEXT, bg, "km", 118, 116);
    textC(166, 108, 1, MUTED, "NEAREST STRIKE");
  } else if (v.mode == LT_CLEAR) {
    snprintf(t, sizeof t, "%u", l->radius);
    ltNum(166, 46, 58, t, TEXT, bg, "km", 118, 116);
    textC(166, 108, 1, MUTED, "ALL CLEAR WITHIN");
  } else {
    ltNum(166, 46, 58, "--", MUTED, bg, nullptr, 118, 116);
    textC(166, 108, 1, MUTED, "NO DATA YET");
  }
  ltTitle(v, bg);
  ltSubtitle(v, t, sizeof t);
  textC(120, 144, 1, MUTED, t);

  // cards: strikes in 10 minutes, where the storm is, where it's heading (all clear: when the last one was)
  ltCard(0, "STRIKES", card);
  if (v.mode == LT_WAIT) textC(43, 196, 3, MUTED, "--");
  else {
    snprintf(t, sizeof t, "%u", v.mode == LT_ACTIVE ? l->n10 : 0);
    ltNum(43, 192, 28, t, TEXT, card, nullptr, 62);
  }
  textC(43, 222, 1, MUTED, "LAST 10 MIN");
  if (v.mode == LT_CLEAR) {
    ltCard(1, "LAST ONE", card);
    if (v.ago == NO_STRIKE || v.ago >= 100UL * 3600) {
      textC(120, 196, 3, MUTED, "--");
      textC(120, 222, 1, MUTED, "NONE SEEN");
    } else {
      const bool hours = v.ago >= 3600;
      snprintf(t, sizeof t, "%lu", (unsigned long)(hours ? v.ago / 3600 : v.ago / 60));
      ltNum(120, 192, 28, t, TEXT, card, nullptr, 62);
      textC(120, 222, 1, MUTED, hours ? "HOURS AGO" : "MIN AGO");
    }
    ltCard(2, "RANGE", card);
    snprintf(t, sizeof t, "%u", l->radius);
    ltNum(197, 192, 28, t, TEXT, card, nullptr, 62);
    textC(197, 222, 1, MUTED, "KM");
  } else {
    ltCard(1, "FROM", card);
    textC(120, 196, 3, v.mode == LT_ACTIVE && l->from >= 0 ? TEXT : MUTED,
          v.mode == LT_ACTIVE && l->from >= 0 ? compass8(l->from) : "--");
    ltCard(2, "MOVING", card);
    textC(197, 196, 3, v.mode == LT_ACTIVE && l->moving >= 0 ? TEXT : MUTED,
          v.mode == LT_ACTIVE && l->moving >= 0 ? compass8(l->moving) : "--");
  }
  if (v.flash) {
    G->drawRoundRect(1, 1, 238, 238, 14, BOLT);
    G->drawRoundRect(2, 2, 236, 236, 13, BOLT);
  }
}

static void paintLtIcon(void *ctx) {
  const LtView &v = *(const LtView *)ctx;
  G->fillRect(14, 36, 100, 86, BG);
  ltIcon(v, BG);
}

static void paintLtTitle(void *ctx) {
  const LtView &v = *(const LtView *)ctx;
  G->fillRect(0, 122, 240, 18, BG);
  ltTitle(v, BG);
}

void themeLightning(const UiState &s, bool full) {
  static uint32_t lastKey = 0;
  static uint32_t lastAnim = 0;
  LtView v = {&s, ltMode(s), false, ltAgo(s)};
  v.flash = ltFlash(s, v.mode);
  // everything that changes the static layout: data, state, flash, the minute, the date and the "min ago" text
  char d[24];
  dateLabel(s, d, sizeof d);
  uint32_t key = (s.lightning ? s.lightning->version : 0) * 2654435761UL;
  key ^= v.mode | (v.flash << 2) | ((s.timeOk ? s.now.tm_hour * 60 + s.now.tm_min : 2000) << 3);
  key ^= (v.ago == NO_STRIKE ? 0xFFFFUL : v.ago / 60) << 15;
  key += s.wx.version * 40503UL + (uint8_t)d[strlen(d) / 2] + s.accent;
  if (full || key != lastKey) {
    lastKey = key;
    lastAnim = s.ms;
    uiPaint(full, 0, 0, 240, 240, paintLightning, &v);
    return;
  }
  if (v.flash || s.ms - lastAnim < 120) return;
  // between redraws, only the storm icon and the glowing title move (~8 frames per second, off-screen)
  lastAnim = s.ms;
  uiPaint(false, 14, 36, 100, 86, paintLtIcon, &v);
  if (v.mode == LT_ACTIVE) uiPaint(false, 0, 122, 240, 18, paintLtTitle, &v);
}
