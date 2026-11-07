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
static void vane(float deg, uint16_t c) {
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

// Faixas de vento: traços que atravessam a bússola no sentido em que o vento sopra. Posições vêm só do
// relógio (ms), sem estado: cada traço tem uma faixa lateral e uma fase próprias.
static void streaks(const UiState &s) {
  const float to = (s.wx.wdir + 180) * K_PI / 180, dx = sinf(to), dy = -cosf(to);  // para onde sopra
  const float speed = 14 + s.wx.wind * 1.6f;                                        // px por segundo
  const int len = (4 + s.wx.wind / 4) > 16 ? 16 : 4 + s.wx.wind / 4;
  const int n = s.wx.wind < 5 ? 8 : 16;
  const float span = 2.0f * (RING - 6);
  for (int i = 0; i < n; i++) {
    const float lane = ((i * 37) % 23 - 11) * 5.6f;                       // afastamento lateral
    const float phase = (i * 0.61803f - (int)(i * 0.61803f)) * span;       // espalha no comprimento
    const float along = fmodf(phase + s.ms / 1000.0f * speed * (0.75f + (i % 4) * 0.12f), span) - span / 2;
    const float x0 = CX - dy * lane + dx * along, y0 = CY + dx * lane + dy * along;
    const float r2 = (x0 - CX) * (x0 - CX) + (y0 - CY) * (y0 - CY);
    if (r2 > (RING - 8) * (RING - 8)) continue;
    const float edge = 1 - fabsf(along) / (span / 2);                      // some nas bordas
    const uint16_t col = mix(BG, LONG, 0.35f + 0.6f * edge);
    G->drawLine(x0, y0, x0 - dx * len, y0 - dy * len, col);
    G->drawLine(x0 + dy, y0 - dx, x0 + dy - dx * len, y0 - dx - dy * len, col);  // 2 px de largura
  }
}

static void paintDial(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(CX - 80, 30, 160, 160, BG);
  const bool ok = s.weatherOn && s.wx.ok;
  if (ok && s.wx.wind) streaks(s);
  dial();
  if (!ok) {
    textC(CX, CY - 4, 2, MUTED, "--");
    return;
  }
  const uint8_t f = beaufort(s.wx.wind);
  if (s.wx.wind) {
    // balança mais quando as rajadas passam bem da média
    float amp = (s.wx.gust > s.wx.wind ? (s.wx.gust - s.wx.wind) * 0.6f : 0) + 2;
    if (amp > 16) amp = 16;
    const float wob = sinf(s.ms / 1000.0f * 2.3f) * amp + sinf(s.ms / 1000.0f * 5.1f) * amp * 0.3f;
    vane(s.wx.wdir + wob, s.accent == TEXT ? LONG : s.accent);
  }
  char t[24];
  snprintf(t, sizeof t, "%u", s.wx.wind);
  textC(CX, CY - 28, 5, TEXT, t);
  textC(CX, CY + 16, 1, MUTED, "km/h");
  snprintf(t, sizeof t, "%u kn", (unsigned)lroundf(s.wx.wind * 0.54f));
  textC(CX, CY + 28, 1, MUTED, t);
  textC(CX, CY - 42, 1, TEXT, beaufortName(f));
}

static void paintCards(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 190, 240, 50, BG);
  if (!(s.weatherOn && s.wx.ok)) {
    textC(CX, 212, 1, MUTED, tr("defina a cidade no painel", "set the city in the panel", "elige la ciudad en el panel"));
    return;
  }
  char v[16], sub[16];
  snprintf(v, sizeof v, "%s", windDir(s.wx.wdir));
  snprintf(sub, sizeof sub, "%u deg", s.wx.wdir);
  card(8, "FROM", v, sub, TEXT);
  snprintf(v, sizeof v, "%u km/h", s.wx.gust);
  snprintf(sub, sizeof sub, "Force %u", beaufort(s.wx.wind));
  card(122, "GUSTS", v, sub, s.wx.gust >= 60 ? WORK : TEXT);
}

static void paintWind(void *c) {
  G->fillRect(0, 24, 240, 216, BG);
  paintDial(c);
  paintCards(c);
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
  static uint32_t lastAnim = 0;
  if (full || lastWx != s.wx.version || lastWxOn != (int)s.weatherOn) {
    lastWx = s.wx.version;
    lastWxOn = s.weatherOn;
    lastAnim = s.ms;
    uiPaint(full, 0, 24, 240, 216, paintWind, (void *)&s);
    return;
  }
  // só a bússola anda: ~8 quadros por segundo, desenhada fora da tela (sem piscar)
  if (s.weatherOn && s.wx.ok && s.wx.wind && s.ms - lastAnim >= 120) {
    lastAnim = s.ms;
    uiPaint(false, CX - 80, 30, 160, 160, paintDial, (void *)&s);
  }
}
