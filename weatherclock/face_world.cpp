// weatherbox: World clock and World weather faces. Up to 6 cities each, picked in the panel (Settings,
// "World clock" and "World weather"). Numbers in Outfit, words in the pixel font.
// The firmware fills s.world: each clock city's UTC offset (from its POSIX time zone, recomputed every
// minute, so daylight saving changes on time) and each weather city's conditions (Open-Meteo, every 20 min).
#include "ui_kit.h"

static const uint16_t BAND_CLOCK = uiRgb(23, 78, 96);
static const uint16_t BAND_WX = uiRgb(31, 83, 123);
static const uint16_t NIGHT = uiRgb(150, 172, 230);
static const int TOP = 54;  // first row (below the date bar and the title band)

// Rows share the space under the title: 1-3 cities get tall rows, 4 medium, 5-6 compact.
static int rowH(int n) { return n <= 3 ? 60 : n == 4 ? 46 : 31; }

// Days since 1970-01-01 for a local date (civil calendar)
static long civilDays(int y, int mo, int d) {
  if (mo < 3) { y--; mo += 12; }
  return 365L * y + y / 4 - y / 100 + y / 400 + (153 * (mo - 3) + 2) / 5 + d - 719469;
}
// The home clock's own offset from UTC right now, from its local time and the UTC seconds.
static long homeOffset(const UiState &s) {
  long local = civilDays(s.now.tm_year + 1900, s.now.tm_mon + 1, s.now.tm_mday) * 86400L +
               s.now.tm_hour * 3600L + s.now.tm_min * 60L + s.now.tm_sec;
  return local - (long)s.utc;
}

// Top bar: date on the left, home time on the right (Outfit), then the title band.
static void header(const UiState &s, const char *title, uint16_t band) {
  G->fillRect(0, 0, 240, TOP, BG);
  char l[24];
  dateLabel(s, l, sizeof l);
  text(10, 7, 1, MUTED, l);
  if (s.timeOk) {
    char hm[8];
    snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
    vecText(VF_OUTFIT, 230 - vecWidth(VF_OUTFIT, hm, 18), 3, 18, hm, s.accent, BG);
  }
  G->fillRect(0, 26, 240, 24, band);
  textC(120, 30, 2, TEXT, title);
}

static void emptyNote(const char *what) {
  textC(120, 104, 2, MUTED, "NO CITIES YET");
  textC(120, 132, 1, MUTED, "Pick them in the panel:");
  textC(120, 146, 1, MUTED, what);
}

// City name in size 2 when it fits in maxW, else size 1 (upper case).
static void upper(const char *name, char *u) {
  int i = 0;
  for (; name[i] && i < 15; i++) u[i] = (name[i] >= 'a' && name[i] <= 'z') ? name[i] - 32 : name[i];
  u[i] = 0;
}
static void cityName(int x, int y, int maxW, const char *name, uint16_t c, bool small) {
  char u[16];
  upper(name, u);
  if (!small && textW(u, 2) <= maxW) text(x, y, 2, c, u);
  else text(x, y + (small ? 0 : 4), 1, c, u);
}
// One size for every name on the face: big only when all of them fit in maxW (mixed sizes look untidy).
static bool namesFit(const UiWorldCity *c, int n, int maxW) {
  char u[16];
  for (int i = 0; i < n; i++) {
    upper(c[i].name, u);
    if (textW(u, 2) > maxW) return false;
  }
  return true;
}

// "+9H", "-2:30H", "SAME TIME"
static void diffText(long diffSec, char *out, size_t n) {
  long m = diffSec / 60;
  if (!m) {
    snprintf(out, n, "SAME TIME");
    return;
  }
  const char sign = m < 0 ? '-' : '+';
  if (m < 0) m = -m;
  if (m % 60) snprintf(out, n, "%c%ld:%02ldH", sign, m / 60, m % 60);
  else snprintf(out, n, "%c%ldH", sign, m / 60);
}

// ---------- World clock ----------
static void paintWorldClock(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  header(s, "WORLD CLOCK", BAND_CLOCK);
  G->fillRect(0, TOP, 240, 240 - TOP, BG);
  const UiWorld *w = s.world;
  if (!w || !w->nc) {
    emptyNote("Settings > World clock");
    return;
  }
  if (!s.timeOk || !s.utc) {
    textC(120, 120, 2, MUTED, "SYNCING");
    return;
  }
  const int n = w->nc, R = rowH(n);
  const int th = R >= 60 ? 34 : R >= 46 ? 28 : 22;
  const bool small = R < 46 || !namesFit(w->c, n, 186 - vecWidth(VF_OUTFIT, s.hour12 ? "12:00" : "00:00", th));
  const long home = homeOffset(s);
  const long homeDays = ((long)s.utc + home) / 86400;
  for (int i = 0; i < n; i++) {
    const UiWorldCity &c = w->c[i];
    const int y = TOP + i * R, cy = y + R / 2;
    if (i) G->drawFastHLine(8, y, 224, CARD2);
    if (!c.offOk) {
      cityName(36, cy - 8, 110, c.name, MUTED, false);
      textR(230, cy - 4, 1, MUTED, "NO TIME ZONE");
      continue;
    }
    const long t = (long)s.utc + c.off;
    const long days = t / 86400, sod = t % 86400;
    const int hh = sod / 3600, mm = sod / 60 % 60, wd = (int)((days + 4) % 7);  // 1970-01-01 was a Thursday
    const bool day = hh >= 6 && hh < 18;
    const float is = R >= 46 ? 0.62f : 0.48f;
    if (day) sunShape(18, cy, is);
    else moonShape(18, cy, is, BG);
    // time: Outfit, right-aligned; in 12-hour mode AM/PM goes on the line below
    int h12 = hh;
    if (s.hour12) h12 = hh % 12 ? hh % 12 : 12;
    char hm[8];
    snprintf(hm, sizeof hm, s.hour12 ? "%d:%02d" : "%02d:%02d", h12, mm);
    const int tw = vecWidth(VF_OUTFIT, hm, th);
    const uint16_t tc = day ? s.accent : NIGHT;
    // the line below the name: weekday there, and the difference from here
    char diff[12], sub[32];
    diffText(c.off - home, diff, sizeof diff);
    const long dd = days - homeDays;
    snprintf(sub, sizeof sub, "%s%s %s%s", WD[wd], dd > 0 ? " TMRW" : dd < 0 ? " YDAY" : "", diff,
             s.hour12 ? (hh < 12 ? " AM" : " PM") : "");
    vecText(VF_OUTFIT, 230 - tw, cy - th / 2, th, hm, tc, BG);
    if (!small) {
      cityName(36, cy - 13, 200, c.name, TEXT, false);
      text(36, cy + 7, 1, MUTED, sub);
    } else {  // name and the line below in the small font
      cityName(36, cy - 10, 200, c.name, TEXT, true);
      text(36, cy + 2, 1, MUTED, sub);
    }
  }
}

void themeWorldClock(const UiState &s, bool full) {
  static uint32_t lastKey = 0xFFFFFFFF;
  const UiWorld *w = s.world;
  // redraw once a minute, or when the cities/offsets change
  const uint32_t key = (s.utc / 60) * 131u + (s.now.tm_hour * 60u + s.now.tm_min) * 977u + s.now.tm_mday * 13u + (w ? w->version : 0) * 7u + s.hour12 + (s.timeOk ? 2 : 0) +
                       (uint32_t)s.accent * 17u;
  if (!full && key == lastKey) return;
  lastKey = key;
  uiPaint(full, 0, 0, 240, 240, paintWorldClock, (void *)&s);
}

// ---------- World weather ----------
static int toUnit(const UiState &s, int c) { return s.fahr ? (c * 9 + (c >= 0 ? 2 : -2)) / 5 + 32 : c; }

static void degNum(int rx, int y, int h, int v, uint16_t col) {
  char t[12];
  snprintf(t, sizeof t, "%d", v < 0 ? -v : v);
  const int r = h / 10 + 1, dw = vecWidth(VF_OUTFIT, t, h);
  int x = rx - 2 * r - 3 - dw;
  vecText(VF_OUTFIT, x, y, h, t, col, BG);
  G->drawCircle(rx - r, y + r + 1, r, col);
  if (v < 0) G->fillRect(x - h / 3 - 2, y + h / 2 - h / 16, h / 3, h / 8 + 1, col);
}

static float iconScale(int R) { return R >= 60 ? 0.85f : R >= 46 ? 0.7f : 0.52f; }

static void paintIcons(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  const UiWorld *w = s.world;
  G->fillRect(0, TOP, 44, 240 - TOP, BG);
  const int n = w->nw, R = rowH(n);
  for (int i = 0; i < n; i++) {
    const UiWorldCity &c = w->w[i];
    const int y = TOP + i * R;
    if (i) G->drawFastHLine(8, y, 36, CARD2);
    if (c.wxOk) weatherIcon(22, y + R / 2, iconScale(R), c.code, c.day, BG, s.ms | 1);
  }
}

static void paintWorldWeather(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  header(s, "WORLD WEATHER", BAND_WX);
  G->fillRect(0, TOP, 240, 240 - TOP, BG);
  const UiWorld *w = s.world;
  if (!w || !w->nw) {
    emptyNote("Settings > World weather");
    return;
  }
  const int n = w->nw, R = rowH(n);
  const bool small = R < 46 || !namesFit(w->w, n, 124);
  for (int i = 0; i < n; i++) {
    const UiWorldCity &c = w->w[i];
    const int y = TOP + i * R, cy = y + R / 2;
    if (i) G->drawFastHLine(8, y, 224, CARD2);
    if (!c.wxOk) {
      cityName(46, cy - 8, 120, c.name, MUTED, false);
      textR(230, cy - 4, 1, MUTED, "LOADING");
      continue;
    }
    weatherIcon(22, cy, iconScale(R), c.code, c.day, BG, s.ms | 1);
    const int th = R >= 60 ? 34 : R >= 46 ? 28 : 22;
    const int tv = toUnit(s, c.t);
    degNum(230, R >= 46 ? cy - th / 2 - (R >= 60 ? 6 : 4) : cy - th / 2, th, tv, TEXT);
    char hl[16], sub[32];
    snprintf(hl, sizeof hl, "H%d L%d", toUnit(s, c.tmax), toUnit(s, c.tmin));
    // the weather in words, and the chance of rain when there is one
    if (c.rain != 255 && c.rain >= 20) snprintf(sub, sizeof sub, "%.12s %u%%", weatherText(c.code), c.rain);
    else snprintf(sub, sizeof sub, "%.16s", weatherText(c.code));
    for (char *p = sub; *p; p++)
      if (*p >= 'a' && *p <= 'z') *p -= 32;
    if (R >= 46) {
      cityName(46, small ? cy - 10 : cy - 13, 200, c.name, TEXT, small);
      text(46, small ? cy + 2 : cy + 7, 1, MUTED, sub);
      textR(230, cy + th / 2 + (R >= 60 ? 0 : 1), 1, MUTED, hl);
    } else {
      // compact: the icon already shows the conditions, so the line below gives the high and low (and rain)
      if (c.rain != 255 && c.rain >= 20) snprintf(sub, sizeof sub, "%s  RAIN %u%%", hl, c.rain);
      else snprintf(sub, sizeof sub, "%s", hl);
      cityName(44, cy - 10, 120, c.name, TEXT, true);
      text(44, cy + 2, 1, MUTED, sub);
    }
  }
}

void themeWorldWeather(const UiState &s, bool full) {
  static uint32_t lastKey = 0xFFFFFFFF, lastAnim = 0;
  const UiWorld *w = s.world;
  const uint32_t key = (s.utc / 60) * 131u + (s.now.tm_hour * 60u + s.now.tm_min) * 977u + s.now.tm_mday * 13u + (w ? w->version : 0) * 7u + s.fahr + (s.timeOk ? 2 : 0) +
                       (uint32_t)s.accent * 17u;
  if (full || key != lastKey) {
    lastKey = key;
    lastAnim = s.ms;
    uiPaint(full, 0, 0, 240, 240, paintWorldWeather, (void *)&s);
    return;
  }
  // only the icon column animates
  if (w && w->nw && s.ms - lastAnim >= 200) {
    lastAnim = s.ms;
    uiPaint(false, 0, TOP, 44, 240 - TOP, paintIcons, (void *)&s);
  }
}
