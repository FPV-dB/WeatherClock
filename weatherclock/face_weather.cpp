// Mostrador Clima: temperatura grande e mini cartões.
// Registrado em faces_list.h; usa o kit de desenho de ui_kit.h.
#include "ui_kit.h"
#include "assets.h"

// ---------- tema 3: CLIMA ----------
// weatherbox: números na Outfit e rótulos em pixel, como no Weather Half. Os dígitos negativos levam um traço.
static void outfitNum(int cx, int y, int h, const char *v, uint16_t c, uint16_t bg, char unit) {
  const bool neg = v[0] == '-';
  const char *d = neg ? v + 1 : v;
  const int dw = vecWidth(VF_OUTFIT, d, h), mw = neg ? h / 3 + 2 : 0;
  const int uw = unit == 'o' ? h / 5 + 4 : unit == '%' ? 12 : 0;
  int x = cx - (mw + dw + uw) / 2;
  if (neg) {
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

static void miniCard(int x, const char *label, const char *value, bool deg) {
  const int y = 160, w = 70, h = 72, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 12, CARD);
  textC(cx, y + 12, 1, MUTED, label);
  outfitNum(cx, y + 32, 28, value, TEXT, CARD, deg ? 'o' : '%');
}

// Bloco do clima (abaixo da barra de cima), desenhado fora da tela e mandado pronto: sem piscar.
static void paintWeather(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 24, 240, 216, BG);
  if (!(s.weatherOn && s.wx.ok)) {
    textC(120, 96, 2, MUTED, s.weatherOn ? tr("Buscando clima", "Fetching weather", "Buscando clima") : tr("Clima desligado", "Weather off", "Clima apagado"));
    textC(120, 124, 1, MUTED, tr("defina a cidade no painel", "set the city in the panel", "elige la ciudad en el panel"));
    return;
  }
  weatherIcon(64, 82, 1.6f, s.wx.code, s.wx.day, BG, s.ms | 1);
  char tv[8];
  snprintf(tv, sizeof tv, "%d", (int)lroundf(s.wx.t));
  outfitNum(166, 46, 58, tv, TEXT, BG, 'o');
  char desc[24];
  snprintf(desc, sizeof desc, "%s", s.wx.rainIn ? rainSoonText(s.wx.rainIn) : weatherText(s.wx.code));
  textC(120, 124, 2, TEXT, desc);
  if (s.city && s.city[0]) {
    char c[24];
    snprintf(c, sizeof c, "%.20s", s.city);
    textC(120, 144, 1, MUTED, c);
  }
  char a[8], b[8], h[8];
  snprintf(a, sizeof a, "%d", (int)lroundf(s.wx.tmin));
  snprintf(b, sizeof b, "%d", (int)lroundf(s.wx.tmax));
  snprintf(h, sizeof h, "%d", s.wx.hum);
  miniCard(8, "MIN", a, true);
  miniCard(85, "MAX", b, true);
  miniCard(162, tr("UMIDADE", "HUMIDITY", "HUMEDAD"), h, false);
}

// weatherbox: só o ícone grande é redesenhado (fora da tela) para animar
static const int IC_X = 12, IC_Y = 36, IC_W = 106, IC_H = 86;
static void paintIcon(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(IC_X, IC_Y, IC_W, IC_H, BG);
  weatherIcon(64, 82, 1.6f, s.wx.code, s.wx.day, BG, s.ms | 1);
}

void themeWeather(const UiState &s, bool full) {
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
  char l[24];
  dateLabel(s, l, sizeof l);
  if (changed(cStatL, l)) {
    uiTextBox(0, 4, 140, 10, BG, 10, 7, 1, MUTED, l, 'l');
  }
  if (changed(cC, hm)) {
    struct T { const char *hm; uint16_t c; } t = {hm, s.accent};
    uiPaint(full, 150, 0, 90, 22, [](void *p) {  // fora da tela: a hora troca sem piscar
      const T &t = *(const T *)p;
      G->fillRect(150, 0, 90, 22, BG);
      vecText(VF_OUTFIT, 230 - vecWidth(VF_OUTFIT, t.hm, 18), 3, 18, t.hm, t.c, BG);
    }, &t);
  }
  static uint32_t lastAnim = 0;
  if (!(full || lastWx != s.wx.version || lastWxOn != (int)s.weatherOn)) {
    if (s.weatherOn && s.wx.ok && s.ms - lastAnim >= 150) {
      lastAnim = s.ms;
      uiPaint(false, IC_X, IC_Y, IC_W, IC_H, paintIcon, (void *)&s);
    }
    return;
  }
  lastWx = s.wx.version;
  lastWxOn = s.weatherOn;
  uiPaint(full, 0, 24, 240, 216, paintWeather, (void *)&s);
}

