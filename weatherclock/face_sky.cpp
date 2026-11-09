// weatherbox: three faces built on the extra Open-Meteo data.
//  - Sky: the sun's path between sunrise and sunset (the moon at night) and the moon phase.
//  - Next 12 hours: temperature graph and chance of rain, hour by hour.
//  - Umbrella: a big yes / maybe / no for today, with feels-like, UV and air quality.
// Registered in faces_list.h; uses the drawing kit in ui_kit.h. Everything is drawn off-screen (no flicker).
#include "ui_kit.h"
#include <ctype.h>
#include <math.h>

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
static int skyBx = -100, skyBy = -100;  // where the sun or moon was drawn (for the animation)
static bool skyDay = true;
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

// ---------- the light of the day: where the sun is and what "hour" that makes ----------
// Sun position from the UTC time and the place (the usual low-precision formulas, good to a few tenths of a degree).
static void sunPosition(uint32_t utc, float lat, float lon, float &el, float &az) {
  const float D2R = K_PI / 180;
  const double n = utc / 86400.0 - 10957.5;  // days since 2000-01-01 12:00 UTC
  const float L = fmod(280.460 + 0.9856474 * n, 360.0), g = fmod(357.528 + 0.9856003 * n, 360.0) * D2R;
  const float lam = (L + 1.915f * sinf(g) + 0.020f * sinf(2 * g)) * D2R, eps = (23.439f - 0.0000004f * (float)n) * D2R;
  const float ra = atan2f(cosf(eps) * sinf(lam), cosf(lam)), dec = asinf(sinf(eps) * sinf(lam));
  const float gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0);
  const float ha = (gmst + lon) * D2R - ra, la = lat * D2R;
  el = asinf(sinf(la) * sinf(dec) + cosf(la) * cosf(dec) * cosf(ha)) / D2R;
  az = atan2f(-sinf(ha), tanf(dec) * cosf(la) - sinf(la) * cosf(ha)) / D2R;
  if (az < 0) az += 360;
}

// 0 night, 1 astronomical twilight, 2 nautical twilight, 3 blue hour, 4 golden hour, 5 daylight
static uint8_t lightPhase(float el) {
  return el >= 6 ? 5 : el >= -4 ? 4 : el >= -6 ? 3 : el >= -12 ? 2 : el >= -18 ? 1 : 0;
}
static const char *lightName(uint8_t p) {
  switch (p) {
    case 5: return tr("DIA CLARO", "DAYLIGHT", "PLENO DIA");
    case 4: return tr("HORA DOURADA", "GOLDEN HOUR", "HORA DORADA");
    case 3: return tr("HORA AZUL", "BLUE HOUR", "HORA AZUL");
    case 2: return tr("CREPUSCULO NAUTICO", "NAUTICAL TWILIGHT", "CREPUSC. NAUTICO");
    case 1: return tr("CREPUSCULO ASTRON.", "ASTRO TWILIGHT", "CREPUSC. ASTRON.");
    default: return tr("NOITE", "NIGHT", "NOCHE");
  }
}
static uint16_t lightColor(uint8_t p) {
  static const uint16_t C[6] = {uiRgb(110, 120, 170), uiRgb(90, 110, 200), uiRgb(80, 130, 230), uiRgb(70, 150, 255),
                                uiRgb(255, 176, 50), uiRgb(255, 220, 90)};
  return C[p < 6 ? p : 0];
}
// Worked out when the body is redrawn (every 5 minutes), not on every animation frame.
static struct { bool ok; float el, az; uint8_t phase, next; int nextMin; } light;
static void lightUpdate(const UiState &s) {
  light.ok = s.utc && (s.lat || s.lon);
  if (!light.ok) return;
  sunPosition(s.utc, s.lat, s.lon, light.el, light.az);
  light.phase = lightPhase(light.el);
  light.nextMin = -1;
  for (int k = 2; k <= 24 * 60; k += 2) {  // the next change, in 2-minute steps
    float e, a;
    sunPosition(s.utc + k * 60, s.lat, s.lon, e, a);
    uint8_t p = lightPhase(e);
    if (p != light.phase) {
      light.next = p;
      light.nextMin = (s.now.tm_hour * 60 + s.now.tm_min + k) % 1440;
      break;
    }
  }
}
static const char *compass16(float az) {
  static const char D[16][4] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  return D[(int)((az + 11.25f) / 22.5f) % 16];
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
    skyBx = bx; skyBy = by; skyDay = day;
    if (day) {
      // the part already travelled, in the sun's colour
      for (int d = 180; d >= (int)(180 * (1 - f)); d -= 6) {
        const float t = d * K_PI / 180;
        G->fillCircle(cx + (int)lroundf(R * cosf(t)), cy - (int)lroundf(R * sinf(t)), 1, uiRgb(150, 110, 30));
      }
      // weatherbox: 8 rays that turn slowly (s.ms) and a soft halo
      const float turn = (s.ms % 6000) * 2 * K_PI / 6000;
      for (int k = 0; k < 8; k++) {
        const float r = turn + k * K_PI / 4;
        thickLine(bx + 14 * cosf(r), by + 14 * sinf(r), bx + 19 * cosf(r), by + 19 * sinf(r), 2, SUN);
      }
      G->fillCircle(bx, by, 12, uiRgb(90, 66, 10));
      G->fillCircle(bx, by, 10, SUN);
    } else if (s.timeOk) {
      // weatherbox: a halo that breathes around the moon
      const int ph = (int)(s.ms / 120 % 16), w = ph < 8 ? ph : 16 - ph;
      G->drawCircle(bx, by, 13 + w / 3, mix16(BG, uiRgb(120, 124, 150), 8 - w));
      moonDisc(bx, by, 10, moonAge(s.now), s.south);
    }
  }
  // the light of the moment, inside the arc: its name, the sun's height and direction, and what comes next
  if (light.ok) {
    const char *n = lightName(light.phase);
    textC(120, 74, textW(n, 2) <= 150 ? 2 : 1, lightColor(light.phase), n);
    char l[40];
    const int e = (int)lroundf(light.el);
    snprintf(l, sizeof l, tr("sol %d  %s", "sun %d  %s", "sol %d  %s"), e, compass16(light.az));
    const int w = textW(l, 1) + 6, x0 = 120 - w / 2;
    char head[12];
    snprintf(head, sizeof head, tr("sol %d", "sun %d", "sol %d"), e);
    text(x0, 96, 1, MUTED, l);
    degree(x0 + textW(head, 1) + 1, 96, 1, MUTED);
    if (light.nextMin >= 0) {
      char hm[12];
      hhmm(s, light.nextMin, hm, sizeof hm);
      snprintf(l, sizeof l, tr("%s as %s", "%s at %s", "%s a las %s"), lightName(light.next), hm);
      for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
      textC(120, 110, 1, mix16(BG, lightColor(light.next), 12), l);
    }
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
  if (bodyDue(s, full, 5)) {
    lightUpdate(s);
    uiPaint(full, 0, 24, 240, 216, paintSky, (void *)&s);
    return;
  }
  // weatherbox: only the box around the sun or moon is redrawn (the whole scene, clipped to it), ~8 per second
  static uint32_t lastAnim = 0;
  if (s.weatherOn && s.wx.ok && skyBx > -50 && s.ms - lastAnim >= 120) {
    lastAnim = s.ms;
    const int x = skyBx - 21 < 0 ? 0 : skyBx - 21, y = skyBy - 21 < 24 ? 24 : skyBy - 21;
    uiPaint(false, x, y, (x + 42 > 240 ? 240 - x : 42), 42, paintSky, (void *)&s);
  }
}

// ---------- Next 12 hours ----------
static int hoursX = -100, hoursY = -100;  // the "now" dot (for the animation)
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
    if (i == 0 && s.ms) {  // weatherbox: a ring grows out of the "now" dot and fades
      const int ph = (int)(s.ms / 100 % 12);
      G->drawCircle(x, y, 4 + ph, mix16(BG, SUN, 12 - ph));
      hoursX = x; hoursY = y;
    }
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
  if (bodyDue(s, full, 60)) {
    uiPaint(full, 0, 24, 240, 216, paintHours, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;  // weatherbox: the ring around "now", redrawn on its own
  if (s.weatherOn && s.wx.ok && hoursX > -50 && s.ms - lastAnim >= 100) {
    lastAnim = s.ms;
    const int x = hoursX - 17 < 0 ? 0 : hoursX - 17;
    uiPaint(false, x, hoursY - 17, 34, 34, paintHours, (void *)&s);
  }
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
  weatherIcon(46, 88, 1.0f, wet || chance >= 50 ? 61 : code, s.wx.day, BG, s.ms | 1);
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
  if (bodyDue(s, full, 60)) {
    uiPaint(full, 0, 24, 240, 216, paintUmbrella, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;  // weatherbox: only the weather icon moves (rain falls, sun turns...)
  if (s.weatherOn && s.wx.ok && s.ms - lastAnim >= 150) {
    lastAnim = s.ms;
    uiPaint(false, 12, 60, 70, 60, paintUmbrella, (void *)&s);
  }
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
// A small sun beside "UV INDEX" whose rays turn and pulse, so the face is visibly alive even at night (UV 0).
static const int SUN_X = 94, SUN_Y = 42;  // its box (81..107, 29..55) stays clear of the card edge and the UV number
static void uvSun(const UiState &s, uint16_t col) {
  const float turn = (s.ms % 4000) * 2 * K_PI / 4000;
  const int ph = (int)(s.ms / 90 % 16), wave = ph < 8 ? ph : 16 - ph;
  for (int k = 0; k < 8; k++) {
    const float r = turn + k * K_PI / 4, len = 9 + (k % 2 ? wave / 3.0f : 1);
    thickLine(SUN_X + 7 * cosf(r), SUN_Y + 7 * sinf(r), SUN_X + len * cosf(r), SUN_Y + len * sinf(r), 2, col);
  }
  G->fillCircle(SUN_X, SUN_Y, 5, col);
}
static void paintSun(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(SUN_X - 13, SUN_Y - 13, 27, 27, CARD);
  uvSun(s, s.wx.uv10 != 255 ? uvColor(s.wx.uv10) : SUN);
}
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
      const int grow = s.ms ? wave / 2 : 0;  // 0..4 px taller
      G->fillRoundRect(x - grow / 2, 102 - grow, 16 + grow, 22 + grow, 3, s.ms ? mix16(col, TEXT, wave) : col);
      const int my = 88 + (s.ms ? wave * 3 / 4 : 0);  // the marker bounces 0..6 px
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
  uvSun(s, s.wx.uv10 != 255 ? uvColor(s.wx.uv10) : SUN);
  const bool uvOk = s.wx.uv10 != 255;
  const int uv = uvOk ? (s.wx.uv10 + 5) / 10 : 0;
  const uint16_t uc = uvOk ? uvColor(s.wx.uv10) : MUTED;
  char v[8];
  snprintf(v, sizeof v, uvOk ? "%d" : "--", uv);
  vecText(VF_OUTFIT, 18, 48, 36, v, uc, CARD);  // ends at y 84, above the animated scale strip (y 86+)
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
  static uint32_t lastAnim = 0;  // weatherbox: only the sun and the scale move, ~10 frames per second
  if (s.weatherOn && s.wx.ok && s.ms - lastAnim >= 90) {
    lastAnim = s.ms;
    uiPaint(false, SUN_X - 13, SUN_Y - 13, 27, 27, paintSun, (void *)&s);
    if (s.wx.uv10 != 255) uiPaint(false, 14, SC_Y, 214, SC_H, paintScale, (void *)&s);
  }
}

// ---------- Moon ----------
// Moon position with the short series (Meeus, low precision): about a degree, plenty for rise/set to a few minutes.
static void moonPosition(uint32_t utc, float lat, float lon, float &el, float &az, float &km) {
  const float D2R = K_PI / 180;
  const double n = utc / 86400.0 - 10957.5;
  const float L = fmod(218.316 + 13.176396 * n, 360.0) * D2R, M = fmod(134.963 + 13.064993 * n, 360.0) * D2R,
              F = fmod(93.272 + 13.229350 * n, 360.0) * D2R;
  const float elon = L + 6.289f * D2R * sinf(M), elat = 5.128f * D2R * sinf(F), eps = (23.439f - 0.0000004f * (float)n) * D2R;
  km = 385001 - 20905 * cosf(M);
  const float ra = atan2f(sinf(elon) * cosf(eps) - tanf(elat) * sinf(eps), cosf(elon));
  const float dec = asinf(sinf(elat) * cosf(eps) + cosf(elat) * sinf(eps) * sinf(elon));
  const float gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0);
  const float ha = (gmst + lon) * D2R - ra, la = lat * D2R;
  el = asinf(sinf(la) * sinf(dec) + cosf(la) * cosf(dec) * cosf(ha)) / D2R;
  az = atan2f(-sinf(ha), tanf(dec) * cosf(la) - sinf(la) * cosf(ha)) / D2R;
  if (az < 0) az += 360;
}

// Worked out when the body is redrawn (every 10 minutes): rise and set within the next 24 h, the dates of the next
// full and new moon, and where the moon is now.
static struct { bool ok; float el, az, km, age; int rise, set; uint32_t fullAt, newAt; int32_t off; } moon;
static void moonUpdate(const UiState &s) {
  static uint32_t key = 0;  // recomputed once per 10-minute slot (a fresh draw in the same slot shows the same times)
  const bool ok = s.utc && s.timeOk && (s.lat || s.lon);
  const uint32_t k = s.utc / 600 + (uint32_t)(s.lat * 100) * 7 + (uint32_t)(s.lon * 100);
  if (ok && moon.ok && k == key) return;
  key = k;
  moon.ok = ok;
  if (!moon.ok) return;
  moonPosition(s.utc, s.lat, s.lon, moon.el, moon.az, moon.km);
  moon.age = moonAge(s.now);
  // local time offset: local midnight-based seconds vs UTC
  long y = s.now.tm_year + 1900, m = s.now.tm_mon + 1, d = s.now.tm_mday;
  if (m < 3) { y--; m += 12; }
  const long days = 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d - 719469;
  const int64_t local = (int64_t)days * 86400 + s.now.tm_hour * 3600 + s.now.tm_min * 60 + s.now.tm_sec;
  moon.off = (int32_t)(local - (int64_t)s.utc);
  moon.rise = moon.set = -1;
  float prev = moon.el;
  for (int k = 4; k <= 24 * 60; k += 4) {  // 4-minute steps over the next day
    float e, a, km;
    moonPosition(s.utc + k * 60, s.lat, s.lon, e, a, km);
    const int at = (s.now.tm_hour * 60 + s.now.tm_min + k) % 1440;
    if (prev < 0 && e >= 0 && moon.rise < 0) moon.rise = at;
    if (prev >= 0 && e < 0 && moon.set < 0) moon.set = at;
    prev = e;
  }
  const float SYN = 29.530589f;
  float toFull = SYN / 2 - moon.age, toNew = SYN - moon.age;
  if (toFull < 0) toFull += SYN;
  moon.fullAt = s.utc + (uint32_t)(toFull * 86400);
  moon.newAt = s.utc + (uint32_t)(toNew * 86400);
}

// "THU 26 NOV" for a UTC time, in local time
static void dayLabel(uint32_t utc, int32_t off, char *out, size_t n) {
  const int64_t t = (int64_t)utc + off;
  long z = (long)(t / 86400) + 719468, era = z / 146097, doe = z - era * 146097;
  long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  long mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
  const int wd = (int)((t / 86400 + 4) % 7);  // 1970-01-01 was a Thursday
  snprintf(out, n, "%s %ld %s", WD[wd], d, MO[m - 1]);
}

static void moonCard(int x, int y, const char *label, const char *value, uint16_t vc) {
  G->fillRoundRect(x, y, 108, 44, 10, CARD);
  textC(x + 54, y + 7, 1, MUTED, label);
  const int vh = 20, vw = vecWidth(VF_OUTFIT_TXT, value, vh);
  if (vw <= 100) vecText(VF_OUTFIT_TXT, x + 54 - vw / 2, y + 19, vh, value, vc, CARD);
  else textC(x + 54, y + 24, 1, vc, value);
}

static const int MOON_X = 62, MOON_Y = 80, MOON_R = 40;
static void paintMoonHalo(const UiState &s) {
  const int ph = (int)(s.ms / 120 % 20), w = ph < 10 ? ph : 20 - ph;  // ~2.4 s breath
  for (int i = 0; i < 3; i++)
    G->drawCircle(MOON_X, MOON_Y, MOON_R + 3 + i * 2 + w / 4, mix16(BG, uiRgb(140, 144, 170), (10 - w) * (3 - i) / 3));
}

static void paintMoon(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (!moon.ok) {
    textC(120, 110, 2, MUTED, tr("Sem hora ou cidade", "No time or city yet", "Sin hora o ciudad"));
    return;
  }
  paintMoonHalo(s);
  moonDisc(MOON_X, MOON_Y, MOON_R, moon.age, s.south);
  // name, % lit and age
  const char *n = moonName(moonPhase(s.now));
  const int nw = vecWidth(VF_OUTFIT_TXT, n, 18);
  if (nw <= 120) vecText(VF_OUTFIT_TXT, 116, 36, 18, n, TEXT, BG);
  else text(116, 40, 1, TEXT, n);
  const int lit = (int)lroundf((1 - cosf(2 * K_PI * moon.age / 29.530589f)) * 50);
  char v[8];
  snprintf(v, sizeof v, "%d", lit);
  vecText(VF_OUTFIT, 116, 60, 34, v, uiRgb(236, 232, 214), BG);
  text(116 + vecWidth(VF_OUTFIT, v, 34) + 4, 82, 2, MUTED, "%");
  text(116, 100, 1, MUTED, tr("iluminada", "lit", "iluminada"));
  char a[24];
  snprintf(a, sizeof a, tr("dia %d de 29,5", "day %d of 29.5", "dia %d de 29,5"), (int)moon.age);
  text(116, 112, 1, MUTED, a);
  // where it is now and how far
  char l[40];
  const int e = (int)lroundf(moon.el);
  if (e >= 0) snprintf(l, sizeof l, tr("agora %d  %s  %ld km", "now up %d  %s  %ld km", "ahora %d  %s  %ld km"), e, compass16(moon.az), (long)moon.km);
  else snprintf(l, sizeof l, tr("abaixo do horizonte  %ld km", "below the horizon  %ld km", "bajo el horizonte  %ld km"), (long)moon.km);
  textC(120, 128, 1, e >= 0 ? TEXT : MUTED, l);
  // rise, set, next full and new moon
  char t[16];
  hhmm(s, moon.rise, t, sizeof t);
  moonCard(8, 142, tr("NASCE", "MOONRISE", "SALE"), moon.rise >= 0 ? t : "--", TEXT);
  hhmm(s, moon.set, t, sizeof t);
  moonCard(124, 142, tr("SE POE", "MOONSET", "SE PONE"), moon.set >= 0 ? t : "--", TEXT);
  dayLabel(moon.fullAt, moon.off, t, sizeof t);
  moonCard(8, 190, tr("LUA CHEIA", "FULL MOON", "LUNA LLENA"), t, SUN);
  dayLabel(moon.newAt, moon.off, t, sizeof t);
  moonCard(124, 190, tr("LUA NOVA", "NEW MOON", "LUNA NUEVA"), t, uiRgb(150, 160, 210));
}
static void paintMoonAnim(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(MOON_X - MOON_R - 12, MOON_Y - MOON_R - 12, 2 * MOON_R + 24, 2 * MOON_R + 24, BG);
  paintMoonHalo(s);
  moonDisc(MOON_X, MOON_Y, MOON_R, moon.age, s.south);
}

void themeMoon(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 10)) {
    moonUpdate(s);
    uiPaint(full, 0, 24, 240, 216, paintMoon, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;  // weatherbox: a soft halo breathes around the moon
  if (moon.ok && s.ms - lastAnim >= 120) {
    lastAnim = s.ms;
    uiPaint(false, MOON_X - MOON_R - 12, MOON_Y - MOON_R - 12, 2 * MOON_R + 24, 2 * MOON_R + 24, paintMoonAnim, (void *)&s);
  }
}

// ---------- Rain ----------
// Today: the chance of rain and how many millimetres; then the next 12 hours (bars = chance, numbers = mm) and the
// coming days. Drops fall in the today card while rain is likely.
static void mmText(uint16_t mm10, char *out, size_t n) {
  if (mm10 == 0) snprintf(out, n, "0");
  else if (mm10 < 100) snprintf(out, n, "%u.%u", mm10 / 10, mm10 % 10);
  else snprintf(out, n, "%u", (mm10 + 5) / 10);
}
// mm in the Outfit digits; the decimal point is drawn (the number font has digits only). Returns the width.
static int mmBig(int x, int y, int h, uint16_t mm10, uint16_t c, uint16_t bg, bool draw = true) {
  char a[8], b[4] = "";
  if (mm10 < 100 && mm10 % 10) { snprintf(a, sizeof a, "%u", mm10 / 10); snprintf(b, sizeof b, "%u", mm10 % 10); }
  else snprintf(a, sizeof a, "%u", (mm10 + 5) / 10);
  const int wa = vecWidth(VF_OUTFIT, a, h), dot = b[0] ? h / 6 + 4 : 0, wb = b[0] ? vecWidth(VF_OUTFIT, b, h) : 0;
  if (draw) {
    vecText(VF_OUTFIT, x, y, h, a, c, bg);
    if (b[0]) {
      G->fillCircle(x + wa + dot / 2, y + h - h / 8, h / 14 + 1, c);
      vecText(VF_OUTFIT, x + wa + dot, y, h, b, c, bg);
    }
  }
  return wa + dot + wb;
}
static int rainChanceToday(const UiState &s) {
  int c = s.wx.nd && s.wx.d[0].rain != 255 ? s.wx.d[0].rain : 0;
  for (int i = 0; i < s.wx.nh; i++)
    if (s.wx.hp[i] != 255) c = imax(c, s.wx.hp[i]);
  return c;
}
static const int RD_X = 8, RD_Y = 28, RD_W = 72, RD_H = 70;  // the drops' box inside the today card
static void rainDrops(const UiState &s, int chance) {
  if (chance < 30) return;
  const int n = chance >= 70 ? 9 : chance >= 50 ? 6 : 4;
  for (int i = 0; i < n; i++) {
    const int lane = 14 + (i * 37) % (RD_W - 16), speed = 3 + i % 3, len = 6 + i % 3;
    const int y = RD_Y + 6 + (int)((s.ms / 40 * speed + i * 23) % (RD_H - 14));
    G->drawFastVLine(RD_X + lane, y, len, RAIN);
    G->drawFastVLine(RD_X + lane + 1, y + 1, len - 2, mix16(CARD, RAIN, 8));
  }
}
static void paintRainDrops(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(RD_X, RD_Y, RD_W, RD_H, CARD);
  weatherIcon(RD_X + RD_W / 2, RD_Y + RD_H / 2 - 4, 0.8f, rainChanceToday(s) >= 30 ? 61 : s.wx.code, s.wx.day, CARD);
  rainDrops(s, rainChanceToday(s));
}

static void paintRain(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (noWeather(s)) return;
  const int chance = rainChanceToday(s);
  const uint16_t today = s.wx.nd ? s.wx.d[0].mm10 : 0;
  // today card
  G->fillRoundRect(8, 28, 224, 70, 14, CARD);
  paintRainDrops(c);
  text(90, 34, 1, MUTED, tr("CHUVA HOJE", "RAIN TODAY", "LLUVIA HOY"));
  char v[12];
  snprintf(v, sizeof v, "%d", chance);
  const uint16_t cc = chance >= 70 ? RAIN : chance >= 30 ? uiRgb(150, 200, 255) : MUTED;
  vecText(VF_OUTFIT, 90, 48, 30, v, cc, CARD);
  int x = 90 + vecWidth(VF_OUTFIT, v, 30) + 2;
  text(x, 66, 1, cc, "%");
  char l[24];
  const int mw = mmBig(0, 0, 26, today, TEXT, CARD, false), mx = 214 - mw;  // right-aligned, "mm" after it
  mmBig(mx, 52, 26, today, TEXT, CARD);
  text(mx + mw + 2, 70, 1, MUTED, "mm");
  if (s.wx.rainIn) snprintf(l, sizeof l, "%s", rainSoonText(s.wx.rainIn));
  else snprintf(l, sizeof l, "%s", tr("chance  /  esperado", "chance  /  expected", "prob.  /  esperado"));
  text(90, 86, 1, s.wx.rainIn ? RAIN : MUTED, l);
  // next 12 hours: bars for the chance, mm above when there's any
  const int n = s.wx.nh;
  text(10, 106, 1, MUTED, tr("PROXIMAS 12 HORAS", "NEXT 12 HOURS", "PROXIMAS 12 HORAS"));
  int tot = 0, top = -1;
  for (int i = 0; i < n; i++) {
    tot += s.wx.hmm[i];
    if (s.wx.hmm[i] && (top < 0 || s.wx.hmm[i] > s.wx.hmm[top])) top = i;
  }
  {
    char t[16], m[8];
    mmText((uint16_t)imin(tot, 9999), m, sizeof m);
    snprintf(t, sizeof t, "%s mm", m);
    textR(230, 106, 1, tot ? uiRgb(150, 200, 255) : MUTED, t);
  }
  if (n >= 2) {
    const int x0 = 12, bw = 216 / n - 3, by1 = 160, bh = 34;
    for (int i = 0; i < n; i++) {
      const int bx = x0 + i * (216 / n), p = s.wx.hp[i] == 255 ? 0 : s.wx.hp[i], h = p * bh / 100;
      G->fillRect(bx, by1 - bh, bw, bh, DIM);
      if (h) G->fillRect(bx, by1 - h, bw, h, s.wx.hmm[i] ? RAIN : mix16(DIM, RAIN, 7));
      if (i == top) {  // the wettest hour gets its mm
        char m[8];
        mmText(s.wx.hmm[i], m, sizeof m);
        const int tw = textW(m, 1);
        text(iclamp(bx + bw / 2 - tw / 2, 2, 238 - tw), by1 - h - 10, 1, TEXT, m);
      }
      if (i % 3 == 0) {
        char hl[8];
        const int hh = (s.wx.h0 + i) % 24;
        if (i == 0) snprintf(hl, sizeof hl, "%s", tr("agora", "now", "ahora"));
        else if (s.hour12) snprintf(hl, sizeof hl, "%d%s", hh % 12 ? hh % 12 : 12, hh < 12 ? "a" : "p");
        else snprintf(hl, sizeof hl, "%02d", hh);
        text(bx, by1 + 3, 1, i == 0 ? SUN : MUTED, hl);
      }
    }
  }
  // the coming days
  const int nd = s.wx.nd > 1 ? imin(s.wx.nd - 1, 4) : 0;
  for (int k = 0; k < nd; k++) {
    const UiDay &d = s.wx.d[k + 1];
    const int cx = 8 + k * 57, cw = 52;
    G->fillRoundRect(cx, 176, cw, 58, 8, CARD);
    const int wd = s.timeOk ? (s.now.tm_wday + k + 1) % 7 : k;
    textC(cx + cw / 2, 181, 1, MUTED, WD[wd]);
    char p[8];
    if (d.rain == 255) snprintf(p, sizeof p, "--");
    else snprintf(p, sizeof p, "%d%%", d.rain);
    textC(cx + cw / 2, 195, 2, d.rain != 255 && d.rain >= 50 ? RAIN : TEXT, p);
    char m[10], mm2[14];
    mmText(d.mm10, m, sizeof m);
    snprintf(mm2, sizeof mm2, "%smm", m);
    textC(cx + cw / 2, 218, 1, d.mm10 ? uiRgb(150, 200, 255) : MUTED, mm2);
  }
}

void themeRain(const UiState &s, bool full) {
  topBar(s, full);
  if (bodyDue(s, full, 60)) {
    uiPaint(full, 0, 24, 240, 216, paintRain, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;  // weatherbox: drops fall in the today card (and the icon moves)
  if (s.weatherOn && s.wx.ok && s.ms - lastAnim >= 100) {
    lastAnim = s.ms;
    uiPaint(false, RD_X + 2, RD_Y + 2, RD_W - 2, RD_H - 4, paintRainDrops, (void *)&s);
  }
}

// ---------- News ----------
// Headlines sent by the computer program (radar/news.py, every 10 minutes): the newest one big, the next ones
// smaller, each with how long ago it was published. A red "live" dot pulses in the header.
static int wrapText(int x, int y, uint8_t sz, uint16_t c, const char *s, int maxChars, int maxLines) {
  const int lh = 8 * sz + 3;
  int lines = 0;
  while (*s && lines < maxLines) {
    while (*s == ' ') s++;
    int len = (int)strlen(s), cut = len;
    if (len > maxChars) {
      cut = maxChars;
      while (cut > 0 && s[cut] != ' ') cut--;
      if (cut == 0) cut = maxChars;
    }
    char line[64];
    int n = cut < (int)sizeof line - 4 ? cut : (int)sizeof line - 4;
    memcpy(line, s, n);
    line[n] = 0;
    if (lines == maxLines - 1 && len > cut) {  // last line and more text: ellipsis
      while (n > 0 && n + 3 > maxChars) line[--n] = 0;
      strcat(line, "...");
    }
    text(x, y, sz, c, line);
    y += lh;
    s += cut;
    lines++;
  }
  return y;
}
static void agoText(uint16_t mins, char *out, size_t n) {
  if (mins < 1) snprintf(out, n, "%s", tr("agora", "just now", "ahora"));
  else if (mins < 60) snprintf(out, n, tr("ha %u min", "%u min ago", "hace %u min"), mins);
  else snprintf(out, n, tr("ha %u h", "%u h ago", "hace %u h"), mins / 60);
}
static const uint16_t NEWS_RED = uiRgb(220, 50, 45);
static void newsDot(const UiState &s) {
  const int ph = (int)(s.ms / 100 % 14), w = ph < 7 ? ph : 14 - ph;
  G->fillCircle(18, 37, 6, NEWS_RED);
  G->fillCircle(18, 37, 2 + w / 2, mix16(NEWS_RED, uiRgb(255, 255, 255), 6 + w));
}
static void paintNewsDot(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(10, 29, 17, 17, NEWS_RED);
  newsDot(s);
}
static void paintNews(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  G->fillRect(0, 28, 240, 19, NEWS_RED);
  newsDot(s);
  text(30, 33, 2, TEXT, tr("NOTICIAS", "NEWS", "NOTICIAS"));
  const UiNews *nw = s.news;
  if (nw && nw->src[0]) textR(232, 34, 1, uiRgb(255, 220, 215), nw->src);
  if (!nw || !nw->n) {
    textC(120, 100, 2, MUTED, tr("Sem noticias", "No news yet", "Sin noticias"));
    textC(120, 126, 1, MUTED, tr("precisa do programa do radar", "needs the radar program", "necesita el programa del radar"));
    textC(120, 138, 1, MUTED, tr("num computador", "on a computer", "en un ordenador"));
    return;
  }
  const uint16_t since = (uint16_t)((s.ms - nw->at) / 60000);
  int y = 54;
  char a[24];
  for (int i = 0; i < nw->n && y < 220; i++) {
    const bool top = i == 0;
    y = wrapText(8, y, top ? 2 : 1, top ? TEXT : uiRgb(205, 205, 212), nw->t[i], top ? 19 : 37, top ? 4 : 2);
    agoText(nw->ago[i] + since, a, sizeof a);
    if (y > 230) break;
    text(8, y, 1, top ? NEWS_RED : MUTED, a);
    y += 13;
    if (i + 1 < nw->n && y < 220) G->drawFastHLine(8, y - 3, 224, CARD2);
    y += 3;
  }
}

void themeNews(const UiState &s, bool full) {
  topBar(s, full);
  static uint16_t lastNews = 0xFFFF;
  const uint16_t v = s.news ? s.news->version : 0;
  if (bodyDue(s, full, 1) || v != lastNews) {  // the minute counts ("12 min ago") move on
    lastNews = v;
    uiPaint(full, 0, 24, 240, 216, paintNews, (void *)&s);
    return;
  }
  static uint32_t lastAnim = 0;
  if (s.ms - lastAnim >= 100) {
    lastAnim = s.ms;
    uiPaint(false, 10, 29, 17, 17, paintNewsDot, (void *)&s);
  }
}
