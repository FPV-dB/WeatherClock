// Mostrador Vento (weatherbox): bússola com o rumo, velocidade, rajadas e força na escala Beaufort.
// Registrado em faces_list.h; usa o kit de desenho de ui_kit.h.
#include "ui_kit.h"
#include "assets.h"

static const int CX = 120, CY = 110, RING = 76;

static void polar(float deg, float r, int &x, int &y) {
  const float a = deg * K_PI / 180;
  x = CX + lroundf(sinf(a) * r);
  y = CY - lroundf(cosf(a) * r);
}

static uint8_t beaufort(int kmh) {
  static const uint8_t LIMIT[] = {1, 6, 12, 20, 29, 39, 50, 62, 75, 89, 103, 118};
  uint8_t f = 0;
  while (f < sizeof LIMIT && kmh >= LIMIT[f]) f++;
  return f;
}

static const char *beaufortName(uint8_t f) {
  static const char *const N[] = {"Calm", "Light air", "Light breeze", "Gentle breeze", "Moderate breeze", "Fresh breeze",
                                  "Strong breeze", "Near gale", "Gale", "Strong gale", "Storm", "Violent storm", "Hurricane"};
  return N[f > 12 ? 12 : f];
}

static void dial() {
  G->drawCircle(CX, CY, RING, CARD2);
  G->drawCircle(CX, CY, RING - 1, CARD2);
  for (int i = 0; i < 16; i++) {
    int x0, y0, x1, y1;
    polar(i * 22.5f, RING - 3, x0, y0);
    polar(i * 22.5f, RING - (i % 4 ? 8 : 12), x1, y1);
    G->drawLine(x0, y0, x1, y1, i % 4 ? DIM : MUTED);
  }
  static const char *const CARD_PT[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; i++) {
    int x, y;
    polar(i * 90.f, RING - 21, x, y);
    textC(x, y - 4, 1, TEXT, CARD_PT[i]);
  }
}

// Seta na borda, do lado de onde o vento vem, apontando para o centro (como um cata-vento)
static void vane(int deg, uint16_t c) {
  int tx, ty, lx, ly, rx, ry;
  polar(deg, RING - 14, tx, ty);
  polar(deg - 7, RING + 2, lx, ly);
  polar(deg + 7, RING + 2, rx, ry);
  G->fillTriangle(tx, ty, lx, ly, rx, ry, c);
}

static void card(int x, const char *label, const char *value, const char *sub, uint16_t c) {
  const int y = 192, w = 110, h = 46, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 10, CARD);
  textC(cx, y + 5, 1, MUTED, label);
  textC(cx, y + 16, 2, c, value);
  if (sub) textC(cx, y + 34, 1, MUTED, sub);
}

static void paintWind(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  dial();
  if (!(s.weatherOn && s.wx.ok)) {
    textC(CX, CY - 4, 2, MUTED, "--");
    textC(CX, 212, 1, MUTED, tr("defina a cidade no painel", "set the city in the panel", "elige la ciudad en el panel"));
    return;
  }
  const uint8_t f = beaufort(s.wx.wind);
  if (s.wx.wind) vane(s.wx.wdir, s.accent == TEXT ? LONG : s.accent);
  char t[24];
  snprintf(t, sizeof t, "%u", s.wx.wind);
  textC(CX, CY - 28, 5, TEXT, t);
  textC(CX, CY + 16, 1, MUTED, "km/h");
  snprintf(t, sizeof t, "%u kn", (unsigned)lroundf(s.wx.wind * 0.54f));
  textC(CX, CY + 28, 1, MUTED, t);
  char v[16], sub[16];
  snprintf(v, sizeof v, "%s", windDir(s.wx.wdir));
  snprintf(sub, sizeof sub, "%u deg", s.wx.wdir);
  card(8, "FROM", v, sub, TEXT);
  snprintf(v, sizeof v, "%u km/h", s.wx.gust);
  snprintf(sub, sizeof sub, "Force %u", f);
  card(122, "GUSTS", v, sub, s.wx.gust >= 60 ? WORK : TEXT);
  textC(CX, CY - 42, 1, TEXT, beaufortName(f));
}

void themeWind(const UiState &s, bool full) {
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
  char l[24];
  dateLabel(s, l, sizeof l);
  if (changed(cStatL, l)) {
    G->fillRect(0, 4, 140, 10, BG);
    text(10, 7, 1, MUTED, l);
  }
  if (changed(cC, hm)) {
    G->fillRect(150, 0, 90, 22, BG);
    textR(230, 4, 2, s.accent, hm);
  }
  if (!(full || lastWx != s.wx.version || lastWxOn != (int)s.weatherOn)) return;
  lastWx = s.wx.version;
  lastWxOn = s.weatherOn;
  uiPaint(full, 0, 24, 240, 216, paintWind, (void *)&s);
}
