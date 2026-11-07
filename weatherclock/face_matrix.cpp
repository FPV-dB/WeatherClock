// Matrix face: pixel-art digits and weather and wind cards.
// Registered in faces_list.h; uses the drawing kit in ui_kit.h.
#include "ui_kit.h"
#include "assets.h"

// ---------- tema 0: MATRIZ ----------
static const TimeLayout TL_CLOCK = {{20, 66, 136, 182}, 30, 8, 116};

static void weatherCard(const UiState &s, int x, int y) {
  const int w = 108, h = 120, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 12, CARD);
  char label[16];
  snprintf(label, sizeof label, "%.15s", s.city && s.city[0] ? s.city : tr("CLIMA", "WEATHER", "CLIMA"));
  text(x + 12, y + 10, 1, MUTED, label);
  if (s.weatherOn && s.wx.ok) {
    weatherIcon(cx, y + 42, 0.8f, s.wx.code, s.wx.day, CARD);
    tempText(cx, y + 70, 3, TEXT, s.wx.t);
    char mm[20];
    snprintf(mm, sizeof mm, "%d / %d", (int)lroundf(s.wx.tmin), (int)lroundf(s.wx.tmax));
    textC(cx, y + 102, 1, MUTED, mm);
  } else {
    textC(cx, y + 50, 3, DIM, "--");
    textC(cx, y + 92, 1, MUTED, s.weatherOn ? tr("buscando...", "loading...", "buscando...") : tr("desligado", "off", "apagado"));
  }
}

// weatherbox: wind and humidity in place of the focus card
static void windCard(const UiState &s, int x, int y) {
  const int w = 108, h = 120, cx = x + w / 2;
  G->fillRoundRect(x, y, w, h, 12, CARD);
  text(x + 12, y + 10, 1, MUTED, tr("VENTO", "WIND", "VIENTO"));
  if (!(s.weatherOn && s.wx.ok)) {
    textC(cx, y + 50, 3, DIM, "--");
    return;
  }
  char n[8];
  snprintf(n, sizeof n, "%u", s.wx.wind);
  textC(cx, y + 28, 4, TEXT, n);
  char d[16];
  snprintf(d, sizeof d, "km/h %s", windDir(s.wx.wdir));
  textC(cx, y + 62, 1, MUTED, d);
  snprintf(d, sizeof d, tr("umid %d%%", "hum %d%%", "hum %d%%"), s.wx.hum);
  textC(cx, y + 82, 1, MUTED, d);
  const int bx = x + 12, bw = w - 24, by = y + 96;
  G->fillRoundRect(bx, by, bw, 8, 4, CARD2);
  int hum = s.wx.hum < 0 ? 0 : s.wx.hum > 100 ? 100 : s.wx.hum;
  int fill = hum * bw / 100;
  if (fill > 0) G->fillRoundRect(bx, by, fill < 8 ? 8 : fill, 8, 4, LONG);
}

static void paintCards(void *c) {  // weatherbox: both cards built off-screen
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 112, 240, 128, BG);
  weatherCard(s, 8, 112);
  windCard(s, 124, 112);
}

void themeMatrix(const UiState &s, bool full) {
  statusBar(s, s.ip ? s.ip : "", BG);
  if (!s.timeOk) {
    drawTime4(TL_CLOCK, -1, -1, -1, -1, true, s.accent, BG);
  } else {
    int h = displayHour(s);
    drawTime4(TL_CLOCK, h / 10, h % 10, s.now.tm_min / 10, s.now.tm_min % 10, s.now.tm_sec % 2 == 0, s.accent, BG);
    // seconds bar: moves a little every second and resets at the minute rollover
    int w = (s.now.tm_sec + 1) * 200 / 60;
    if (w < lastSecW || lastSecW < 0) {
      G->fillRect(20, 96, 200, 3, DIM);
      lastSecW = 0;
    }
    if (w > lastSecW) {
      G->fillRect(20 + lastSecW, 96, w - lastSecW, 3, s.accent);
      lastSecW = w;
    }
  }
  if (full || lastWx != s.wx.version || lastWxOn != (int)s.weatherOn) {
    uiPaint(full, 0, 112, 240, 128, paintCards, (void *)&s);
    lastWx = s.wx.version;
    lastWxOn = s.weatherOn;
  }
}

