// Mostrador Previsão LCD: a previsão de 4 dias no visor do LCD retrô, com dígitos DSEG7 (segmentos
// apagados à mostra), pictogramas monocromáticos de relógio de pulso e o dia de hoje invertido.
// Registrado em faces_list.h; usa o kit de desenho de ui_kit.h.
#include "ui_kit.h"

// mesmas cores do LCD retrô (face_lcd.cpp)
static const uint16_t LCD_BG = uiRgb(166, 180, 140), LCD_ON = uiRgb(18, 24, 16), LCD_GHOST = uiRgb(161, 175, 136);
static const int ROW_Y = 66, ROW_H = 36;

// Pictogramas de 12 x 10 (desenhados com pixels de 2 x 2), '#' = segmento aceso
static const char PIC[7][10][13] PROGMEM = {
  {"....#..#....", ".#........#.", "....####....", "...######...", "#..######..#",  // sol
   "...######...", "....####....", ".#........#.", "....#..#....", "............"},
  {"#.#.........", ".###........", "#####.###...", ".###.#####..", "#.#.#######.",  // poucas nuvens
   "...#########", "...#########", "....#######.", "............", "............"},
  {"............", "....####....", "..########..", ".##########.", "############",  // nublado
   "############", ".##########.", "............", "............", "............"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // chuva
   "............", "..#..#..#...", ".#..#..#....", "#..#..#.....", "............"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // tempestade
   "......##....", ".....##.....", "....####....", ".....##.....", "....##......"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // neve
   "............", ".#...#...#..", "............", "...#...#...#", "............"},
  {"............", "############", "............", ".##########.", "............",  // neblina
   "############", "............", ".##########.", "............", "............"},
};

static int picFor(int c) {
  if (c == 0) return 0;
  if (c <= 2) return 1;
  if (c == 45 || c == 48) return 6;
  if (c >= 95) return 4;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return 5;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return 3;
  return 2;
}

// pictograma: acesos em LCD_ON e apagados em fantasma, como os ícones fixos de um LCD real
// weatherbox: com phase > 0, segmentos acendem e apagam como num LCD animado de verdade
static bool pictoOn(int p, int r, int c, uint32_t phase) {
  if (!phase) return true;
  switch (p) {
    case 0: {  // sol: raios retos e diagonais se alternam
      const bool core = r >= 2 && r <= 6 && c >= 3 && c <= 8;
      if (core) return true;
      const bool diag = r == 1 || r == 7;
      return (phase % 2) ? diag : !diag;
    }
    case 1: return r > 2 || c > 3 || phase % 2;            // poucas nuvens: os raios piscam
    case 3: return r < 6 || r - 6 == (int)(phase % 3);     // chuva: uma fileira de gotas por vez, descendo
    case 4: return r < 5 || phase % 4 < 2;                 // tempestade: o raio pisca
    case 5: return r < 6 || (r == 6) == (phase % 2 == 0);  // neve: flocos alternam de altura
    case 6: return !(r == 3 || r == 7) || phase % 2;       // neblina: faixas curtas piscam
  }
  return true;
}

static void picto(int x, int y, int p, uint32_t phase = 0) {
  for (int r = 0; r < 10; r++)
    for (int c = 0; c < 12; c++)
      G->fillRect(x + c * 2, y + r * 2, 2, 2, pgm_read_byte(&PIC[p][r][c]) == '#' && pictoOn(p, r, c, phase) ? LCD_ON : LCD_GHOST);
}

// weatherbox: só a coluna dos pictogramas, redesenhada para animar
static const int PIC_X = 62, PIC_W = 24;
static void paintPictos(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(PIC_X, ROW_Y - 4, PIC_W, 4 * ROW_H + 2, LCD_BG);
  for (int i = 0; i < 4 && i < s.wx.nd; i++) {
    const int y = ROW_Y + i * ROW_H, cy = y + ROW_H / 2 - 2;
    picto(PIC_X, cy - 10, picFor(s.wx.d[i].code), s.ms / 400 + 1);
    if (i < 3) G->drawFastHLine(PIC_X, y + ROW_H - 2, PIC_W, uiRgb(140, 154, 116));
  }
}

// número de 2 dígitos em DSEG7 e grau; negativo leva o "-" na casa da dezena, como num LCD real
// (vai até -9). y = topo, h = altura
static void dsegTemp(int x, int y, int h, int v) {
  if (v < -9) v = -9;
  if (v > 199) v = 199;
  if (v >= 100) {  // °F em dia quente: o "1" das centenas e dígitos 3/4 menores, um pouco à esquerda
    y += h / 4;    // mesma linha de base
    h = h * 3 / 4;
    x -= h / 3;
  }
  const int w = h * 618 / 1000, gap = h / 8 + 1;
  if (v >= 100) {
    dsegDigit(x, y, h, 1, LCD_ON, LCD_GHOST);
    x += w + gap;
    v -= 100;
    dsegDigit(x, y, h, v / 10, LCD_ON, LCD_GHOST);  // 104: o zero do meio aparece
  } else
    dsegDigit(x, y, h, v < 0 ? 10 : v >= 10 ? v / 10 : -1, LCD_ON, LCD_GHOST);
  dsegDigit(x + w + gap, y, h, (v < 0 ? -v : v) % 10, LCD_ON, LCD_GHOST);
  int dx = x + 2 * w + gap + 4, r = h > 20 ? 3 : 2;
  G->drawCircle(dx, y + r, r, LCD_ON);
}

static void paintRows(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(14, ROW_Y - 4, 212, 4 * ROW_H + 2, LCD_BG);
  for (int i = 0; i < 4 && i < s.wx.nd; i++) {
    const UiDay &d = s.wx.d[i];
    const int y = ROW_Y + i * ROW_H, cy = y + ROW_H / 2 - 2;
    const char *name = WD[(s.now.tm_wday + i) % 7];
    if (!i) {  // hoje: o nome em negativo, como o dia marcado num relógio de pulso
      uiTextBox(16, cy - 10, 40, 20, LCD_ON, 36, cy - 7, 2, LCD_BG, name, 'c');
    } else textC(36, cy - 7, 2, LCD_ON, name);
    picto(62, cy - 10, picFor(d.code), s.ms / 400 + 1);
    dsegTemp(104, cy - 12, 24, d.tmax);
    dsegTemp(152, cy - 6, 14, d.tmin);
    if (d.rain != 255) {
      char r[6];
      snprintf(r, sizeof r, "%d%%", d.rain);
      G->fillCircle(186, cy + 2, 2, LCD_ON);
      G->fillTriangle(184, cy + 1, 188, cy + 1, 186, cy - 3, LCD_ON);
      text(192, cy - 3, 1, d.rain >= 10 ? LCD_ON : uiRgb(110, 122, 94), r);
    }
    if (i < 3) G->drawFastHLine(18, y + ROW_H - 2, 204, uiRgb(140, 154, 116));
  }
}

void themeForecastLcd(const UiState &s, bool full) {
  if (full) {
    textC(120, 9, 1, uiRgb(170, 170, 182), tr("PREVISAO   4 DIAS", "FORECAST   4 DAYS", "PRONOSTICO   4 DIAS"));
    G->fillRoundRect(8, 24, 224, 186, 14, LCD_BG);
    G->drawFastHLine(22, 25, 196, uiRgb(140, 154, 116));  // sombra interna
    G->drawFastHLine(22, 26, 196, uiRgb(152, 165, 128));
    textC(119, 216, 1, uiRgb(170, 170, 182), "MAX");
    textC(161, 216, 1, uiRgb(170, 170, 182), "MIN");
  }
  // linha de cima: cidade e hora em DSEG7 (segmentos fantasma: troca sem apagar)
  char t[24];
  snprintf(t, sizeof t, "%.13s", s.city && s.city[0] ? s.city : tr("PREVISAO", "FORECAST", "PRONOSTICO"));
  for (char *p = t; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p -= 32;
  if (changed(cB, t)) {
    uiTextBox(16, 34, 132, 16, LCD_BG, 20, 38, 1, LCD_ON, t, 'l');
  }
  int v[4] = {-1, -1, -1, -1};
  if (s.timeOk) {
    int h = displayHour(s);
    v[0] = h >= 10 ? h / 10 : -1; v[1] = h % 10; v[2] = s.now.tm_min / 10; v[3] = s.now.tm_min % 10;
  }
  const int X[4] = {144, 161, 186, 203};  // dígitos de 22 px (13,6 de largura); dois-pontos em x=181
  for (int i = 0; i < 4; i++)
    if (full || lastDig[i] != v[i]) {
      dsegDigit(X[i], 32, 22, v[i], LCD_ON, LCD_GHOST);
      lastDig[i] = v[i];
    }
  if (full) {
    G->fillRect(181, 37, 2, 2, LCD_ON);
    G->fillRect(181, 46, 2, 2, LCD_ON);
    G->drawFastHLine(18, 58, 204, LCD_ON);
  }
  char key[24];
  snprintf(key, sizeof key, "%u %d %d %d", (unsigned)s.wx.version, s.weatherOn, s.wx.ok, s.timeOk ? s.now.tm_wday : -1);
  static uint32_t lastAnim = 0;
  if (!changed(cA, key) && !full) {
    if (s.weatherOn && s.wx.ok && s.wx.nd && s.ms - lastAnim >= 200) {
      lastAnim = s.ms;
      uiPaint(false, PIC_X, ROW_Y - 4, PIC_W, 4 * ROW_H + 2, paintPictos, (void *)&s);
    }
    return;
  }
  if (!(s.weatherOn && s.wx.ok && s.wx.nd)) {
    uiTextBox(14, ROW_Y - 4, 212, 4 * ROW_H + 2, LCD_BG, 120, 120, 2, LCD_ON, s.weatherOn ? tr("BUSCANDO", "FETCHING", "BUSCANDO") : tr("SEM CLIMA", "NO WEATHER", "SIN CLIMA"), 'c');
    return;
  }
  uiPaint(full, 14, ROW_Y - 4, 212, 4 * ROW_H + 2, paintRows, (void *)&s);
}
