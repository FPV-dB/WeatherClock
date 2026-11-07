// Forecast LCD face: the 4-day forecast on the Retro LCD screen, with DSEG7 digits (unlit
// segments visible), monochrome wristwatch pictograms and today shown inverted.
// Registered in faces_list.h; uses the drawing kit in ui_kit.h.
#include "ui_kit.h"

// same colours as the Retro LCD (face_lcd.cpp)
static const uint16_t LCD_BG = uiRgb(166, 180, 140), LCD_ON = uiRgb(18, 24, 16), LCD_GHOST = uiRgb(161, 175, 136);
static const int ROW_Y = 66, ROW_H = 36;

// 12 x 10 pictograms (drawn with 2 x 2 pixels), '#' = lit segment
static const char PIC[7][10][13] PROGMEM = {
  {"....#..#....", ".#........#.", "....####....", "...######...", "#..######..#",  // sun
   "...######...", "....####....", ".#........#.", "....#..#....", "............"},
  {"#.#.........", ".###........", "#####.###...", ".###.#####..", "#.#.#######.",  // poucas nuvens
   "...#########", "...#########", "....#######.", "............", "............"},
  {"............", "....####....", "..########..", ".##########.", "############",  // nublado
   "############", ".##########.", "............", "............", "............"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // rain
   "............", "..#..#..#...", ".#..#..#....", "#..#..#.....", "............"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // storm
   "......##....", ".....##.....", "....####....", ".....##.....", "....##......"},
  {"....####....", "..########..", ".##########.", "############", ".##########.",  // snow
   "............", ".#...#...#..", "............", "...#...#...#", "............"},
  {"............", "############", "............", ".##########.", "............",  // fog
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

// pictogram: lit pixels in LCD_ON and unlit ones as ghosts, like the fixed icons of a real LCD
// weatherbox: with phase > 0, segments light up and go out like a real animated LCD
static bool pictoOn(int p, int r, int c, uint32_t phase) {
  if (!phase) return true;
  switch (p) {
    case 0: {  // sun: straight and diagonal rays alternate
      const bool core = r >= 2 && r <= 6 && c >= 3 && c <= 8;
      if (core) return true;
      const bool diag = r == 1 || r == 7;
      return (phase % 2) ? diag : !diag;
    }
    case 1: return r > 2 || c > 3 || phase % 2;            // partly cloudy: the rays blink
    case 3: return r < 6 || r - 6 == (int)(phase % 3);     // rain: one row of drops at a time, going down
    case 4: return r < 5 || phase % 4 < 2;                 // storm: the lightning flashes
    case 5: return r < 6 || (r == 6) == (phase % 2 == 0);  // snow: flakes alternate height
    case 6: return !(r == 3 || r == 7) || phase % 2;       // fog: short bands blink
  }
  return true;
}

static void picto(int x, int y, int p, uint32_t phase = 0) {
  for (int r = 0; r < 10; r++)
    for (int c = 0; c < 12; c++)
      G->fillRect(x + c * 2, y + r * 2, 2, 2, pgm_read_byte(&PIC[p][r][c]) == '#' && pictoOn(p, r, c, phase) ? LCD_ON : LCD_GHOST);
}

// weatherbox: only the pictogram column, redrawn to animate
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

// 2-digit DSEG7 number and degree sign; a negative puts the "-" in the tens place, like a real LCD
// (goes down to -9). y = top, h = height
static void dsegTemp(int x, int y, int h, int v) {
  if (v < -9) v = -9;
  if (v > 199) v = 199;
  if (v >= 100) {  // °F on a hot day: the hundreds "1" and 3/4-size digits, a little to the left
    y += h / 4;    // same baseline
    h = h * 3 / 4;
    x -= h / 3;
  }
  const int w = h * 618 / 1000, gap = h / 8 + 1;
  if (v >= 100) {
    dsegDigit(x, y, h, 1, LCD_ON, LCD_GHOST);
    x += w + gap;
    v -= 100;
    dsegDigit(x, y, h, v / 10, LCD_ON, LCD_GHOST);  // 104: the middle zero shows
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
    if (!i) {  // today: the name inverted, like the marked day on a wristwatch
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
  // top line: city and time in DSEG7 (ghost segments: changes without erasing)
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
  const int X[4] = {144, 161, 186, 203};  // 22 px digits (13.6 wide); colon at x=181
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
