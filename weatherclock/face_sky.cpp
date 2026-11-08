// weatherbox: three faces built on the extra Open-Meteo data.
//  - Sky: the sun's path between sunrise and sunset (the moon at night) and the moon phase.
//  - Next 12 hours: temperature graph and chance of rain, hour by hour.
//  - Umbrella: a big yes / maybe / no for today, with feels-like, UV and air quality.
// Registered in faces_list.h; uses the drawing kit in ui_kit.h. Everything is drawn off-screen (no flicker).
#include "ui_kit.h"

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iclamp(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static const uint16_t GOOD = uiRgb(62, 207, 122), WARN = uiRgb(255, 196, 40);

// Outfit number centred on cx, with a dash for negatives and an optional degree sign or %.
static void bigNum(int cx, int y, int h, int v, uint16_t c, uint16_t bg, char unit) {
  char d[8];
  snprintf(d, sizeof d, "%d", v < 0 ? -v : v);
  const int dw = vecWidth(VF_OUTFIT, d, h), mw = v < 0 ? h / 3 + 2 : 0;
  const int uw = unit == 'o' ? h / 5 + 4 : unit == '%' ? 12 : 0;
  int x = cx - (mw + dw + uw) / 2;
  if (v < 0) {
    G->fillRect(x, y + h / 2 - h / 16, h / 3, h / 8 + 1, c);
    x += mw;
  }
  vecText(VF_OUTFIT, x, y, h, d, c, bg);
  x += dw + 2;
  if (unit == 'o') {
    const int r = h / 10 + 1;
    G->drawCircle(x + r, y + r + 1, r, c);
    if (h > 30) G->drawCircle(x + r, y + r + 1, r - 1, c);
  } else if (unit == '%') text(x, y + h - 16, 2, c, "%");
}

// Date on the left and the time on the right, like the Weather face. Redrawn only when they change.
static void topBar(const UiState &s, bool full) {
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
  char l[24];
  dateLabel(s, l, sizeof l);
  if (changed(cStatL, l)) uiTextBox(0, 4, 140, 10, BG, 10, 7, 1, MUTED, l, 'l');
  if (changed(cC, hm)) {
    struct T { const char *hm; uint16_t c; } t = {hm, s.accent};
    uiPaint(full, 150, 0, 90, 22, [](void *p) {
      const T &t = *(const T *)p;
      G->fillRect(150, 0, 90, 22, BG);
      vecText(VF_OUTFIT, 230 - vecWidth(VF_OUTFIT, t.hm, 18), 3, 18, t.hm, t.c, BG);
    }, &t);
  }
}

static bool noWeather(const UiState &s) {
  if (s.weatherOn && s.wx.ok) return false;
  textC(120, 110, 2, MUTED, s.weatherOn ? tr("Buscando clima", "Fetching weather", "Buscando clima") : tr("Clima desligado", "Weather off", "Clima apagado"));
  textC(120, 138, 1, MUTED, tr("defina a cidade no painel", "set the city in the panel", "elige la ciudad en el panel"));
  return true;
}

// Redraw the body when the weather changes, the weather is switched on/off, or every `everyMin` minutes.
static bool bodyDue(const UiState &s, bool full, int everyMin) {
  static int lastSlot = -1;
  int slot = s.timeOk ? (s.now.tm_hour * 60 + s.now.tm_min) / everyMin : 0;
  if (!(full || lastWx != s.wx.version || lastWxOn != (int)s.weatherOn || slot != lastSlot)) return false;
  lastWx = s.wx.version;
  lastWxOn = s.weatherOn;
  lastSlot = slot;
  return true;
}

static void hhmm(const UiState &s, int m, char *out, size_t n) {
  if (m < 0) { snprintf(out, n, "--:--"); return; }
  int h = m / 60;
  if (s.hour12) h = h % 12 ? h % 12 : 12;
  snprintf(out, n, "%d:%02d", h, m % 60);
}

// ---------- Sky ----------
// Moon disc of radius r: lit part by the moon's age; mirrored in the southern hemisphere.
static void moonDisc(int cx, int cy, int r, float age, bool south) {
  const float p = age / 29.530589f, k = cosf(2 * K_PI * p);
  const uint16_t lit = uiRgb(236, 232, 214), dark = uiRgb(46, 48, 58);
  for (int dy = -r; dy <= r; dy++) {
    const float w = sqrtf((float)(r * r - dy * dy));
    float a, b;  // lit span, seen from the northern hemisphere: waxing lit on the right
    if (p < 0.5f) { a = w * k; b = w; } else { a = -w; b = -w * k; }
    if (south) { float t = -b; b = -a; a = t; }
    const int x0 = cx - (int)lroundf(w), x1 = cx + (int)lroundf(w);
    G->drawFastHLine(x0, cy + dy, x1 - x0 + 1, dark);
    const int l0 = cx + (int)lroundf(a), l1 = cx + (int)lroundf(b);
    if (l1 > l0) G->drawFastHLine(l0, cy + dy, l1 - l0 + 1, lit);
  }
  G->drawCircle(cx, cy, r, uiRgb(90, 92, 104));
}

static void paintSky(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (noWeather(s)) return;
  const int cx = 120, cy = 132, R = 92;
  // the path: a dotted half circle and the horizon
  for (int d = 0; d <= 180; d += 6) {
    const float a = d * K_PI / 180;
    G->fillCircle(cx + (int)lroundf(R * cosf(a)), cy - (int)lroundf(R * sinf(a)), 1, CARD2);
  }
  G->drawFastHLine(14, cy, 212, MUTED);
  // where the sun (by day) or the moon (by night) is along the path
  const int now = s.timeOk ? s.now.tm_hour * 60 + s.now.tm_min : 12 * 60;
  const int rise = s.wx.rise, set = s.wx.set;
  if (rise >= 0 && set > rise) {
    const bool day = now >= rise && now < set;
    float f;
    if (day) f = (float)(now - rise) / (set - rise);
    else {
      const int len = 1440 - (set - rise), since = now >= set ? now - set : now + 1440 - set;
      f = (float)since / len;
    }
    const float a = K_PI * (1 - f);
    const int bx = cx + (int)lroundf(R * cosf(a)), by = cy - (int)lroundf(R * sinf(a));
    if (day) {
      // the part already travelled, in the sun's colour
      for (int d = 180; d >= (int)(180 * (1 - f)); d -= 6) {
        const float t = d * K_PI / 180;
        G->fillCircle(cx + (int)lroundf(R * cosf(t)), cy - (int)lroundf(R * sinf(t)), 1, uiRgb(150, 110, 30));
      }
      G->fillCircle(bx, by, 13, uiRgb(90, 66, 10));
      G->fillCircle(bx, by, 10, SUN);
    } else if (s.timeOk) moonDisc(bx, by, 10, moonAge(s.now), s.south);
  }
  // sunrise and sunset under the horizon
  char t[12];
  text(14, cy + 8, 1, MUTED, tr("NASCER", "SUNRISE", "SALIDA"));
  hhmm(s, rise, t, sizeof t);
  vecText(VF_OUTFIT, 14, cy + 20, 22, t, TEXT, BG);
  textR(226, cy + 8, 1, MUTED, tr("POR DO SOL", "SUNSET", "PUESTA"));
  hhmm(s, set, t, sizeof t);
  vecText(VF_OUTFIT, 226 - vecWidth(VF_OUTFIT, t, 22), cy + 20, 22, t, TEXT, BG);
  // the moon phase at the bottom
  if (s.timeOk) {
    G->fillRoundRect(8, 184, 224, 48, 12, CARD);
    moonDisc(34, 208, 16, moonAge(s.now), s.south);
    const char *n = moonName(moonPhase(s.now));
    const uint8_t sz = textW(n, 2) <= 176 ? 2 : 1;
    text(60, sz == 2 ? 194 : 198, sz, TEXT, n);
    char d[24];
    snprintf(d, sizeof d, tr("lua de %d dias", "moon %d days old", "luna de %d dias"), (int)moonAge(s.now));
    text(60, 216, 1, MUTED, d);
  }
}

void themeSky(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 5)) uiPaint(full, 0, 24, 240, 216, paintSky, (void *)&s);
}

// ---------- Next 12 hours ----------
static void paintHours(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (noWeather(s)) return;
  textC(120, 30, 1, MUTED, tr("PROXIMAS 12 HORAS", "NEXT 12 HOURS", "PROXIMAS 12 HORAS"));
  const int n = s.wx.nh;
  if (n < 2) {
    textC(120, 110, 2, MUTED, tr("Sem previsao", "No forecast", "Sin pronostico"));
    return;
  }
  int lo = s.wx.ht[0], hi = s.wx.ht[0];
  for (int i = 1; i < n; i++) { lo = imin(lo, s.wx.ht[i]); hi = imax(hi, s.wx.ht[i]); }
  if (hi - lo < 4) { hi += (4 - (hi - lo)) / 2 + 1; lo = hi - 5; }
  const int x0 = 20, x1 = 220, gy0 = 64, gy1 = 132;  // temperature graph
  auto X = [&](int i) { return x0 + (x1 - x0) * i / (n - 1); };
  auto Y = [&](int v) { return gy1 - (gy1 - gy0) * (v - lo) / (hi - lo); };
  for (int i = 0; i + 1 < n; i++) thickLine(X(i), Y(s.wx.ht[i]), X(i + 1), Y(s.wx.ht[i + 1]), 3, s.accent);
  for (int i = 0; i < n; i++) {
    const int x = X(i), y = Y(s.wx.ht[i]);
    G->fillCircle(x, y, 3, i == 0 ? SUN : s.accent);
    if (i % 3 == 0 || i == n - 1) {  // a label every 3 hours
      char v[8];
      snprintf(v, sizeof v, "%d", s.wx.ht[i]);
      const int w = textW(v, 1) + 4;
      int lx = iclamp(x - w / 2, 2, 238 - w);
      text(lx, y - 14, 1, TEXT, v);
      degree(lx + textW(v, 1) + 1, y - 14, 1, TEXT);
    }
  }
  // rain chance bars
  const int by1 = 210, bh = 50, bw = (x1 - x0) / n - 3;
  text(x0 - 12, 146, 1, MUTED, tr("CHUVA", "RAIN", "LLUVIA"));
  int top = 0;
  for (int i = 0; i < n; i++) {
    const int p = s.wx.hp[i] == 255 ? 0 : s.wx.hp[i];
    top = imax(top, p);
    const int x = X(i) - bw / 2, h = p * bh / 100;
    G->fillRect(x, by1 - bh, bw, bh, DIM);
    if (h) G->fillRect(x, by1 - h, bw, h, RAIN);
  }
  char m[16];
  snprintf(m, sizeof m, tr("max %d%%", "max %d%%", "max %d%%"), top);
  textR(x1 + 8, 146, 1, top >= 50 ? RAIN : MUTED, m);
  // hour labels: every 3 hours
  for (int i = 0; i < n; i += 3) {
    int h = (s.wx.h0 + i) % 24;
    char l[8];
    if (i == 0) snprintf(l, sizeof l, "%s", tr("agora", "now", "ahora"));
    else if (s.hour12) snprintf(l, sizeof l, "%d%s", h % 12 ? h % 12 : 12, h < 12 ? "am" : "pm");
    else snprintf(l, sizeof l, "%02d", h);
    textC(X(i), 218, 1, i == 0 ? SUN : MUTED, l);
  }
}

void themeHours(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 60)) uiPaint(full, 0, 24, 240, 216, paintHours, (void *)&s);
}

// ---------- Umbrella ----------
static void miniCard(int x, const char *label, const char *value, uint16_t vc, const char *note, uint16_t nc) {
  const int y = 168, w = 70, h = 64, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 12, CARD);
  textC(cx, y + 8, 1, MUTED, label);
  const int vh = 24, vw = vecWidth(VF_OUTFIT, value, vh);
  vecText(VF_OUTFIT, cx - vw / 2, y + 20, vh, value, vc, CARD);
  textC(cx, y + 50, 1, nc, note);
}

static void paintUmbrella(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (noWeather(s)) return;
  // the chance: the highest of the next hours (or today's, without the hourly forecast)
  int chance = s.wx.nd && s.wx.d[0].rain != 255 ? s.wx.d[0].rain : 0;
  for (int i = 0; i < s.wx.nh; i++)
    if (s.wx.hp[i] != 255) chance = i == 0 ? s.wx.hp[i] : imax(chance, s.wx.hp[i]);
  const int code = s.wx.code;
  const bool wet = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95;
  const char *ans;
  uint16_t col;
  if (wet || chance >= 50) { ans = tr("SIM", "YES", "SI"); col = RAIN; }
  else if (chance >= 25) { ans = tr("TALVEZ", "MAYBE", "QUIZAS"); col = WARN; }
  else { ans = tr("NAO", "NO", "NO"); col = GOOD; }
  textC(120, 30, 2, MUTED, tr("Guarda-chuva hoje?", "Umbrella today?", "Paraguas hoy?"));
  weatherIcon(46, 88, 1.0f, wet || chance >= 50 ? 61 : code, s.wx.day, BG);
  const int ah = 52, aw = vecWidth(VF_OUTFIT_TXT, ans, ah);
  vecText(VF_OUTFIT_TXT, 152 - aw / 2, 60, ah, ans, col, BG);
  char line[32];
  if (wet) snprintf(line, sizeof line, "%s", tr("chovendo agora", "raining now", "lloviendo ahora"));
  else snprintf(line, sizeof line, tr("%d%% de chance de chuva", "%d%% chance of rain", "%d%% prob. de lluvia"), chance);
  textC(120, 130, 1, TEXT, line);
  if (s.wx.rainIn) textC(120, 144, 1, RAIN, rainSoonText(s.wx.rainIn));
  // feels-like, UV and air quality
  char f[8], u[8], a[8];
  snprintf(f, sizeof f, "%d", (int)lroundf(s.wx.feels));
  if (s.wx.uv10 == 255) snprintf(u, sizeof u, "--"); else snprintf(u, sizeof u, "%d", (s.wx.uv10 + 5) / 10);
  if (s.wx.aqi == 0xFFFF) snprintf(a, sizeof a, "--"); else snprintf(a, sizeof a, "%u", s.wx.aqi);
  char fd[8];
  snprintf(fd, sizeof fd, "%d", (int)lroundf(s.wx.t));
  char fn[16];
  snprintf(fn, sizeof fn, tr("real %s", "actual %s", "real %s"), fd);
  miniCard(8, tr("SENSACAO", "FEELS LIKE", "SENSACION"), f, TEXT, fn, MUTED);
  miniCard(85, "UV", u, s.wx.uv10 == 255 ? MUTED : uvColor(s.wx.uv10), uvText(s.wx.uv10), s.wx.uv10 == 255 ? MUTED : uvColor(s.wx.uv10));
  miniCard(162, tr("AR", "AIR", "AIRE"), a, s.wx.aqi == 0xFFFF ? MUTED : aqiColor(s.wx.aqi), aqiText(s.wx.aqi), s.wx.aqi == 0xFFFF ? MUTED : aqiColor(s.wx.aqi));
}

void themeUmbrella(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 60)) uiPaint(full, 0, 24, 240, 216, paintUmbrella, (void *)&s);
}

// ---------- Air & UV ----------
// UV index with its level, a coloured 11-step scale and today's peak; below, PM2.5, PM10 and the air quality index.
static void airCard(int x, const char *label, uint16_t v10, uint8_t level, bool known, const char *unit) {
  const int y = 140, w = 70, h = 92, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 12, CARD);
  textC(cx, y + 8, 1, MUTED, label);
  char v[8];
  if (!known) snprintf(v, sizeof v, "--");
  else snprintf(v, sizeof v, "%u", (unsigned)((v10 + 5) / 10));
  const uint16_t c = known ? levelColor(level) : MUTED;
  const int vh = 30, vw = vecWidth(VF_OUTFIT, v, vh);
  vecText(VF_OUTFIT, cx - vw / 2, y + 22, vh, v, c, CARD);
  textC(cx, y + 58, 1, MUTED, unit);
  if (known) textC(cx, y + 74, 1, c, levelText(level));
}

// The UV scale: 11 steps (11 = 11+), coloured by level. The current step breathes and a marker bounces
// above it (weatherbox: animated, redrawn off-screen on its own).
static const int SC_Y = 86, SC_H = 40;
static void uvScale(const UiState &s) {
  const bool uvOk = s.wx.uv10 != 255;
  const int uv = uvOk ? (s.wx.uv10 + 5) / 10 : 0;
  const uint32_t t = s.ms ? s.ms : 0;
  const int ph = (int)(t / 90 % 16), wave = ph < 8 ? ph : 16 - ph;  // 0..8..0, about 1.4 s
  for (int i = 1; i <= 11; i++) {
    const int x = 18 + (i - 1) * 19, on = uvOk && (uv >= i || (i == 11 && uv > 11));
    const uint16_t col = uvColor(i * 10 - 5);
    const bool cur = uvOk && (uv == i || (i == 11 && uv > 11));
    if (cur) {
      const int grow = s.ms ? wave / 3 : 0;  // 0..2 px taller
      G->fillRoundRect(x - grow / 2, 102 - grow, 16 + grow, 22 + grow, 3, s.ms ? mix16(col, TEXT, wave / 2) : col);
      const int my = 90 + (s.ms ? (wave < 4 ? wave : 8 - wave) : 0);  // the marker bounces 0..4 px
      G->fillTriangle(x + 3, my, x + 13, my, x + 8, my + 6, TEXT);
    } else G->fillRoundRect(x, 106, 16, 14, 3, on ? col : mix16(CARD, col, 4));
  }
}
static void paintScale(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(14, SC_Y, 214, SC_H, CARD);
  uvScale(s);
}

static void paintAir(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (noWeather(s)) return;
  // UV
  G->fillRoundRect(8, 28, 224, 104, 14, CARD);
  text(18, 36, 1, MUTED, tr("INDICE UV", "UV INDEX", "INDICE UV"));
  const bool uvOk = s.wx.uv10 != 255;
  const int uv = uvOk ? (s.wx.uv10 + 5) / 10 : 0;
  const uint16_t uc = uvOk ? uvColor(s.wx.uv10) : MUTED;
  char v[8];
  snprintf(v, sizeof v, uvOk ? "%d" : "--", uv);
  vecText(VF_OUTFIT, 18, 50, 44, v, uc, CARD);
  if (uvOk) {
    const char *t = uvText(s.wx.uv10);
    textR(222, 52, textW(t, 2) <= 120 ? 2 : 1, uc, t);
  }
  if (s.wx.uvMax10 != 255) {
    char m[24];
    snprintf(m, sizeof m, tr("pico hoje %d", "today's peak %d", "maximo hoy %d"), (s.wx.uvMax10 + 5) / 10);
    textR(222, 76, 1, MUTED, m);
  }
  uvScale(s);
  // particles and the index
  airCard(8, "PM2.5", s.wx.pm25, pmLevel(s.wx.pm25, true), s.wx.pm25 != 0xFFFF, "ug/m3");
  airCard(85, "PM10", s.wx.pm10, pmLevel(s.wx.pm10, false), s.wx.pm10 != 0xFFFF, "ug/m3");
  const bool aqOk = s.wx.aqi != 0xFFFF;
  const uint8_t aql = !aqOk ? 0 : s.wx.aqi <= 50 ? 0 : s.wx.aqi <= 100 ? 1 : s.wx.aqi <= 150 ? 2 : s.wx.aqi <= 200 ? 3 : 4;
  airCard(162, tr("INDICE AR", "AIR INDEX", "INDICE AIRE"), aqOk ? s.wx.aqi * 10 : 0, aql, aqOk, "US AQI");
}

void themeAir(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 15)) {
    uiPaint(full, 0, 24, 240, 216, paintAir, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;  // weatherbox: only the scale moves, ~10 frames per second
  if (s.weatherOn && s.wx.ok && s.wx.uv10 != 255 && s.ms - lastAnim >= 90) {
    lastAnim = s.ms;
    uiPaint(false, 14, SC_Y, 214, SC_H, paintScale, (void *)&s);
  }
}
