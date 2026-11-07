// Mostrador Previsão em cartões: cidade e hora em cima e quatro cartões (hoje e os próximos 3 dias)
// com ícone, máxima grande, mínima e chance de chuva. Todo o texto na fonte vetorial Outfit
// (suavizada) e o tempo nos sprites em pixel art da cena Tomate (sol, nuvem, nuvem de chuva).
// Registrado em faces_list.h; usa o kit de desenho de ui_kit.h.
#include "ui_kit.h"
#include "assets.h"

static const int CARD_Y = 34, CARD_W = 54, CARD_H = 200, CARD_GAP = 4;
static const uint16_t CARD_BG = uiRgb(24, 24, 28), CARD_LINE = uiRgb(48, 48, 54);

// texto Outfit centralizado em cx (h = altura da linha, com as descendentes)
static void otxC(int cx, int y, int h, uint16_t c, const char *s, uint16_t bg) {
  vecText(VF_OUTFIT_TXT, cx - vecWidth(VF_OUTFIT_TXT, s, h) / 2, y, h, s, c, bg);
}

// Temperatura centralizada em cx: sinal de menos e grau desenhados (a fonte dos números só tem dígitos)
static void cardTemp(int cx, int y, int h, int v, uint16_t fg) {
  if (v >= 100) {  // °F em dia quente: três dígitos menores para caber no cartão, na mesma linha de base
    y += h * 3 / 10;
    h = h * 7 / 10;
  }
  char d[4];
  snprintf(d, sizeof d, "%d", v < 0 ? -v : v);
  const int minus = v < 0 ? h * 3 / 8 : 0, r = h > 20 ? 3 : 2;
  const int w = minus + vecWidth(VF_OUTFIT, d, h) + 2 * r + 2;
  int x = cx - w / 2;
  if (minus) G->fillRect(x, y + h / 2 - 1, minus - 2, h > 20 ? 3 : 2, fg);
  vecText(VF_OUTFIT, x + minus, y, h, d, fg, CARD_BG);
  int dx = x + w - r - 1, dy = y + h / 8 + r;
  G->drawCircle(dx, dy, r, fg);
  if (h > 20) G->drawCircle(dx, dy, r - 1, fg);
}

// gota de chuva: bico em cima, fundo redondo
static void drop(int x, int y, uint16_t c) {
  G->fillCircle(x, y + 4, 3, c);
  G->fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y - 2, c);
}

// Tempo em pixel art (pixels de 2x2), centrado em (cx, cy), numa área de ~40 x 36
static void pixelWeather(int cx, int cy, int c, uint32_t ms = 0) {
  const bool rain = (c >= 51 && c <= 67) || (c >= 80 && c <= 82), snow = (c >= 71 && c <= 77) || c == 85 || c == 86;
  const bool storm = c >= 95, fog = c == 45 || c == 48;
  auto px = [](int x, int y, uint16_t col) { G->fillRect(x, y, 2, 2, col); };
  // weatherbox: com ms > 0 anima em passos de pixel art (2 px): sol sobe e desce, nuvem vai e vem,
  // gotas e flocos caem, neblina desliza e o raio pisca
  const int step = ms ? (int)(ms / 250 % 4) : 0;
  const int bob = ms ? (step == 1 ? -2 : step == 3 ? 2 : 0) : 0;   // ida e volta de 2 px
  const int fall = ms ? (int)(ms / 180 % 4) * 2 : 0;               // 0, 2, 4, 6 px para baixo
  if (c == 0) {
    uiSprite(&SPR_SUN, cx - 16, cy - 16 + bob, 2);
    return;
  }
  if (c <= 2) {  // poucas nuvens: sol atrás, nuvem na frente
    uiSprite(&SPR_SUN, cx - 20, cy - 20, 2);
    uiSprite(&SPR_CLOUD, cx - 10 + bob, cy - 2, 2);
    return;
  }
  const UiSprite *cloud = rain || storm ? &SPR_RAIN_CLOUD : &SPR_CLOUD;
  const int top = rain || snow || storm || fog ? cy - 16 : cy - 9;
  const int y0 = top + 20;
  if (rain || storm)  // gotas em diagonal, como na cena Tomate
    for (int i = 0; i < 4; i++) {
      if (storm && i == 2) continue;  // lugar do raio
      int x = cx - 12 + i * 8, d = ms ? (fall + i * 2) % 8 : 0;
      px(x - d / 2, y0 + d, RAIN);
      px(x - 2 - d / 2, y0 + 4 + d, RAIN);
      px(x - 4 - d / 2, y0 + 8 + d, RAIN);
    }
  if (snow)
    for (int i = 0; i < 3; i++) {
      int d = ms ? (fall + i * 2) % 8 : 0;
      px(cx - 10 + i * 10 + (d & 2 ? 2 : 0), y0 + 1 + d, TEXT);
      px(cx - 5 + i * 10 - (d & 2 ? 2 : 0), y0 + 8 + d, TEXT);
    }
  uiSprite(cloud, cx - 16 + (fog ? 0 : bob), top, 2);  // a nuvem por cima das gotas
  if (fog)
    for (int i = 0; i < 2; i++) G->fillRect(cx - 14 + i * 6 + (i ? -bob : bob), y0 + 2 + i * 6, 24, 2, DCLOUD);
  if (storm && (!ms || step < 2)) {  // raio em degraus de pixel (pisca)
    static const int8_t B[][2] = {{2, 0}, {1, 1}, {0, 2}, {1, 2}, {2, 2}, {1, 3}, {0, 4}};
    for (auto &b : B) G->fillRect(cx + 2 + b[0] * 3, y0 - 2 + b[1] * 3, 3, 3, SUN);
  }
}

// weatherbox: faixa dos ícones (todas as colunas), redesenhada fora da tela para animar
static const int ICON_Y = CARD_Y + 28, ICON_H = 56;
static void paintIcons(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, ICON_Y, 240, ICON_H, BG);
  for (int i = 0; i < 4 && i < s.wx.nd; i++) {
    const int x = 6 + i * (CARD_W + CARD_GAP), cx = x + CARD_W / 2;
    G->fillRect(x, ICON_Y, CARD_W, ICON_H, CARD_BG);
    G->drawFastVLine(x, ICON_Y, ICON_H, CARD_LINE);
    G->drawFastVLine(x + CARD_W - 1, ICON_Y, ICON_H, CARD_LINE);
    pixelWeather(cx, CARD_Y + 54, s.wx.d[i].code, s.ms | 1);
  }
}

static void paintCards(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, CARD_Y, 240, CARD_H, BG);
  for (int i = 0; i < 4 && i < s.wx.nd; i++) {
    const UiDay &d = s.wx.d[i];
    const int x = 6 + i * (CARD_W + CARD_GAP), cx = x + CARD_W / 2, y = CARD_Y;
    G->fillRoundRect(x, y, CARD_W, CARD_H, 10, CARD_BG);
    G->drawRoundRect(x, y, CARD_W, CARD_H, 10, CARD_LINE);
    otxC(cx, y + 8, 15, i ? MUTED : RAIN, i ? WD[(s.now.tm_wday + i) % 7] : tr("HOJE", "TODAY", "HOY"), CARD_BG);
    pixelWeather(cx, y + 54, d.code);
    cardTemp(cx, y + 96, 24, d.tmax, TEXT);
    cardTemp(cx, y + 132, 15, d.tmin, MUTED);
    if (d.rain != 255 && d.rain >= 10) {
      char r[6];
      snprintf(r, sizeof r, "%d%%", d.rain);
      const int h = 14, w = 9 + vecWidth(VF_OUTFIT_TXT, r, h), rx = cx - w / 2;
      drop(rx + 3, y + 170, RAIN);
      vecText(VF_OUTFIT_TXT, rx + 9, y + 167, h, r, RAIN, CARD_BG);
    }
  }
}

// cidade: a maior altura (até 24 px) em que o nome cabe ao lado da hora
static void paintCity(void *c) {
  const char *t = (const char *)c;
  G->fillRect(0, 2, 160, 30, BG);
  int h = 24;
  while (h > 12 && vecWidth(VF_OUTFIT_TXT, t, h) > 150) h--;
  vecText(VF_OUTFIT_TXT, 8, 6 + (24 - h) / 2, h, t, TEXT, BG);
}
static void paintTime(void *c) {
  const char *hm = (const char *)c;
  G->fillRect(160, 2, 80, 30, BG);
  vecText(VF_OUTFIT, 232 - vecWidth(VF_OUTFIT, hm, 18), 9, 18, hm, MUTED, BG);
}

void themeForecastCards(const UiState &s, bool full) {
  char t[24];
  snprintf(t, sizeof t, "%.23s", s.city && s.city[0] ? s.city : tr("Previsao", "Forecast", "Pronostico"));
  if (changed(cB, t)) uiPaint(full, 0, 2, 160, 30, paintCity, t);
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%d:%02d", displayHour(s), s.now.tm_min);
  if (changed(cC, hm)) uiPaint(full, 160, 2, 80, 30, paintTime, hm);
  // cartões: só quando o clima muda, o dia vira ou o clima liga/desliga
  char key[24];
  snprintf(key, sizeof key, "%u %d %d %d", (unsigned)s.wx.version, s.weatherOn, s.wx.ok, s.timeOk ? s.now.tm_wday : -1);
  static uint32_t lastAnim = 0;
  if (!changed(cA, key) && !full) {
    if (s.weatherOn && s.wx.ok && s.wx.nd && s.ms - lastAnim >= 125) {
      lastAnim = s.ms;
      uiPaint(false, 0, ICON_Y, 240, ICON_H, paintIcons, (void *)&s);
    }
    return;
  }
  if (!(s.weatherOn && s.wx.ok && s.wx.nd)) {
    G->fillRect(0, CARD_Y, 240, CARD_H, BG);
    otxC(120, 104, 20, MUTED, s.weatherOn ? tr("Buscando clima", "Fetching weather", "Buscando clima") : tr("Clima desligado", "Weather off", "Clima apagado"), BG);
    otxC(120, 132, 14, MUTED, tr("defina a cidade no painel", "set the city in the panel", "elige la ciudad en el panel"), BG);
    return;
  }
  uiPaint(full, 0, CARD_Y, 240, CARD_H, paintCards, (void *)&s);
}
