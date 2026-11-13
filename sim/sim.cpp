// Weather Clock screen simulator: runs the firmware's ui.cpp on a 240x240 framebuffer
// and writes each screen to out/<name>.raw (RGB565). png.js converts them to PNG.
//
// It also tests incremental redrawing: draws a sequence of states (seconds
// ticking, minute rollover, new weather, focus finished) and compares the final result
// with a drawing made from scratch. Any different pixel = drawing leftovers on the screen.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define UI_ASSET_LIST  // assets.h includes the list with each asset's name (for the preview sheet)
#include "../weatherclock/ui_kit.h"
#include "../weatherclock/assets.h"
#include "../weatherclock/facepack.h"
#include <chrono>
#include <dirent.h>
#include <vector>
#include <string>
#include <set>

class Frame : public Adafruit_GFX {
 public:
  uint16_t px[240 * 240];
  // flicker: during a redraw, a pixel that showed a colour that is neither the before nor the
  // after colour (e.g. the background between erasing and redrawing the hand). log keeps (position << 16 | colour).
  std::vector<uint32_t> *log = nullptr;
  Frame() : Adafruit_GFX(240, 240) { memset(px, 0, sizeof px); }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= 240 || y >= 240) return;
    px[y * 240 + x] = c;
    if (log) log->push_back((uint32_t)(y * 240 + x) << 16 | c);
  }
};

static void save(const Frame &f, const char *name) {
  char path[128];
  snprintf(path, sizeof path, "out/%s.raw", name);
  FILE *fp = fopen(path, "wb");
  if (!fp) {  // no out/ folder (build.sh creates it)
    perror(path);
    exit(1);
  }
  fwrite(f.px, 2, 240 * 240, fp);
  fclose(fp);
}

static UiState base(int h, int m, int s) {
  UiState st;
  memset(&st, 0, sizeof st);
  st.timeOk = true;
  st.now.tm_year = 126; st.now.tm_mon = 9; st.now.tm_mday = 20; st.now.tm_wday = 2;
  st.now.tm_hour = h; st.now.tm_min = m; st.now.tm_sec = s;
  st.now.tm_isdst = -1;
  st.longEvery = 4;
  st.dailyGoal = 8;
  st.today = 3;
  st.streak = 5;  // plant with a green tomato and the flame in the preview
  st.best = 9;
  st.weatherOn = true;
  st.wx = {true, 23.4f, 14.2f, 27.1f, 44, 2, true, 1, 5, {{2, 14, 27, 10}, {61, 15, 21, 80}, {95, 13, 19, 100}, {3, -2, 22, 20}, {0, 12, 29, 0}}};
  st.wx.wind = 18; st.wx.gust = 31; st.wx.wdir = 225;  // weatherbox: SW wind
  st.wx.feels = 24.6f; st.wx.uv10 = 72; st.wx.aqi = 38; st.wx.uvMax10 = 94; st.wx.pm25 = 58; st.wx.pm10 = 84; st.wx.rise = 5 * 60 + 52; st.wx.set = 19 * 60 + 41;
  {
    static const int8_t T[12] = {23, 24, 25, 26, 26, 25, 24, 22, 20, 19, 18, 17};
    static const uint8_t P[12] = {5, 5, 10, 20, 45, 70, 80, 55, 30, 15, 10, 5};
    st.wx.nh = 12; st.wx.h0 = h;
    memcpy(st.wx.ht, T, 12); memcpy(st.wx.hp, P, 12);
    static const uint8_t MM[12] = {0, 0, 0, 1, 4, 12, 18, 6, 2, 0, 0, 0};
    memcpy(st.wx.hmm, MM, 12);
    st.wx.d[0].mm10 = 43; st.wx.d[1].mm10 = 128; st.wx.d[2].mm10 = 215; st.wx.d[3].mm10 = 6; st.wx.d[4].mm10 = 0;
  }
  st.south = true;
  {
    static UiNews nw = {4, "ABC Just In", {"Arena 'electric' as 36ers debutant hits buzzer beater",
                                            "Two survivors of fatal helicopter crash in Indonesia named",
                                            "Labuschagne axed from Aussie squad for New Zealand Tests",
                                            "Trump says he was 'kidding' about offering Dana White press secretary job"},
                        {42, 44, 72, 135}, 0, 1};
    st.news = &nw;
  }
  // weatherbox: Adelaide, and the matching UTC (local time is ACDT, UTC+10:30)
  st.lat = -34.85f; st.lon = 138.54f;
  {
    int y = st.now.tm_year + 1900, mo = st.now.tm_mon + 1, d = st.now.tm_mday;
    if (mo < 3) { y--; mo += 12; }
    long days = 365L * y + y / 4 - y / 100 + y / 400 + (153 * (mo - 3) + 2) / 5 + d - 719469;
    st.utc = (uint32_t)(days * 86400L + h * 3600L + m * 60L + s - 37800L);
  }
  st.city = getenv("SIM_CITY") ? getenv("SIM_CITY") : "Florianopolis";
  st.accent = uiRgb(242, 242, 242);
  st.ip = "192.168.1.101";
  return st;
}

// Screen of any size (asset preview sheet)
class Canvas : public Adafruit_GFX {
 public:
  int w, h;
  uint16_t *px;
  Canvas(int w_, int h_, uint16_t bg) : Adafruit_GFX(w_, h_), w(w_), h(h_), px(new uint16_t[w_ * h_]) {
    for (int i = 0; i < w * h; i++) px[i] = bg;
  }
  ~Canvas() { delete[] px; }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    px[y * w + x] = c;
  }
};

// Sheet with every asset (each animation frame side by side) -> out/assets.bin
// (header: width and height as uint16; then the RGB565 pixels). png.js writes out/assets.png.
static void assetSheet() {
  const int SC = 4, CW = 124, CH = 112, COLS = 6;
  const int n = sizeof UI_ASSETS / sizeof UI_ASSETS[0];
  // still sprites first; each animation starts on a new row
  for (int draw = 0, cells = 0; draw < 2; draw++) {
    Canvas *c = nullptr;
    if (draw) {
      c = new Canvas(COLS * CW + 8, (cells + COLS - 1) / COLS * CH + 8, BG);
      uiBegin(c);
    }
    int k = 0;
    for (int anim = 0; anim < 2; anim++)
      for (int i = 0; i < n; i++) {
        const UiSprite *sp = UI_ASSETS[i].sprite;
        if ((sp->frames > 1) != (anim == 1)) continue;
        if (anim && k % COLS) k += COLS - k % COLS;
        for (int f = 0; f < sp->frames; f++, k++) {
          if (!c) continue;
          int x = 8 + k % COLS * CW, y = 8 + k / COLS * CH;
          c->fillRoundRect(x, y, CW - 8, CH - 8, 8, UI_ASSETS[i].bg >= 0 ? UI_ASSETS[i].bg : CARD);
          uiSprite(sp, x + (CW - 8 - sp->w * SC) / 2, y + 6 + (80 - sp->h * SC) / 2, SC, f);
          char label[32];
          if (f == 0) snprintf(label, sizeof label, "%s", UI_ASSETS[i].name);
          else snprintf(label, sizeof label, "frame %d", f + 1);
          textC(x + (CW - 8) / 2, y + CH - 22, 1, f ? MUTED : TEXT, label);
        }
      }
    cells = k;
    if (!c) continue;
    FILE *fp = fopen("out/assets.bin", "wb");
    uint16_t wh[2] = {(uint16_t)c->w, (uint16_t)c->h};
    fwrite(wh, 2, 2, fp);
    fwrite(c->px, 2, c->w * c->h, fp);
    fclose(fp);
    delete c;
  }
}

static Frame *F;
static Frame *animFrame = nullptr;
static int animN = 0;
static void saveAnimFrame() {
  char n[32];
  snprintf(n, sizeof n, "flip_anim_%02d", animN++);
  save(*animFrame, n);
}

static void shot(const char *name, const UiState &s, bool pomo) {
  Frame *f = new Frame();
  uiBegin(f);
  if (pomo) uiRenderPomodoro(s);
  else uiRenderClock(s);
  save(*f, name);
  delete f;
}

// ---- modelled ST7789 panel: 320-row RAM + hardware scrolling ----
// Models what main.cpp (tftScroll/tftClearMargin) assumes about the chip, in both orientations: row address =
// y (+80 rotated), MY mirrors the RAM (319 - address) and displayed row n shows RAM row n + VSCSAD.
// VSCSAD comes from uiScrollSad (the same as the firmware). Every scroll command checks the whole view.
struct Panel : public Adafruit_GFX {
  uint16_t ram[320 * 240];  // by row address
  bool flip = false;
  int sad = 0;
  long blitBytes = 0, cmds = 0, bad = 0, badAt = -1;
  const Frame *A = nullptr, *B = nullptr;
  bool mid = false, finished = false;
  Panel() : Adafruit_GFX(240, 240) {}
  void put(int x, int y, uint16_t c) {  // y as the library receives it (0..239); the row offset is its own
    int a = y + (flip ? 80 : 0);
    if (x >= 0 && x < 240 && y >= 0 && y < 240) ram[a * 240 + x] = c;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { put(x, y, c); }
  uint16_t view(int x, int v) const {
    int n = flip ? 239 - v : v, ring = (n + sad) % 320, a = flip ? 319 - ring : ring;
    return ram[a * 240 + x];
  }
  // margin = addresses outside the library's 240 rows; black once, like tftClearMargin
  void clearMargin() { for (int a = flip ? 0 : 240; a < (flip ? 80 : 320); a++) for (int x = 0; x < 240; x++) ram[a * 240 + x] = 0; }
  // expected view with the content L rows up: old, black gap, new
  void check(int L, bool wantB) {
    long d = 0;
    for (int v = 0; v < 240; v++)
      for (int x = 0; x < 240; x++) {
        int y = v + L;
        uint16_t e = wantB ? B->px[v * 240 + x] : y < 240 ? A->px[y * 240 + x] : y < 320 ? 0 : B->px[(y - 320) * 240 + x];
        d += view(x, v) != e;
      }
    if (d) { bad += d; if (badAt < 0) badAt = L; }
  }
};
static Panel *PN;
static void simBlit(int16_t x, int16_t y, uint16_t *px, int16_t w, int16_t h) {  // = tftBitmap
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++) PN->put(x + i, y + r, px[r * w + i]);
  PN->blitBytes += (long)w * h * 2;
}
static void simScroll(int16_t rows, int16_t off) {
  int r = rows ? rows : 320;
  PN->sad = uiScrollSad(PN->flip, r, off);
  PN->cmds++;
  if (!rows) { PN->check(0, PN->finished = true); return; }
  if (off) PN->mid = true;
  PN->check(off, PN->mid && !off);  // offset 0 after scrolling = already back at the start, with the new one
}

// Animated change by hardware scrolling: each face -> the next, in both orientations. On every
// scroll command the view must be old / black gap / new; at the end, offset 0 and exactly the
// screen a full drawing of the new face would give. Measures the cost against a full drawing.
static int checkSlide() {
  int fails = 0;
  long cmds = 0, bytes = 0;
  double tSlide = 0, tFull = 0;
  Panel *p = new Panel();
  Frame *a = new Frame(), *b = new Frame();
  PN = p;
  for (int flip = 0; flip < 2; flip++)
    for (int t = 0; t < THEME_COUNT; t++) {
      int u = (t + 1) % THEME_COUNT;
      UiState sa = base(14, 58, 50), sb = base(14, 58, 50);
      sa.theme = t;
      sb.theme = u;
      uiScrollHw = nullptr;
      uiBlit = nullptr;
      uiBegin(a); uiRenderClock(sa);
      auto t0 = std::chrono::steady_clock::now();
      uiBegin(b); uiRenderClock(sb);
      tFull += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      for (auto &px : p->ram) px = 0xA5A5;  // garbage, like the panel's RAM at power-on
      p->flip = flip; p->sad = 0; p->blitBytes = p->cmds = p->bad = 0; p->badAt = -1; p->mid = p->finished = false;
      p->A = a; p->B = b;
      p->clearMargin();
      uiScrollHw = simScroll;
      uiBlit = simBlit;
      uiBegin(p);
      uiRenderClock(sa);
      bool okA = true;
      for (int v = 0; v < 240; v++) for (int x = 0; x < 240; x++) okA &= p->view(x, v) == a->px[v * 240 + x];
      uiInvalidateSlide();
      t0 = std::chrono::steady_clock::now();
      uiRenderClock(sb);
      tSlide += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      p->check(0, true);
      uiRenderClock(sb);  // incremental frame after the change: nothing changes
      p->check(0, true);
      cmds += p->cmds; bytes += p->blitBytes;
      if (!okA || p->bad || !p->finished || p->sad || uiScrollActive() || p->cmds < 20) {
        printf("scroll %s%s -> %s: %ld wrong pixels (1st at offset %ld), %ld commands, before ok=%d, end=%d, sad=%d, active=%d\n", flip ? "rotated " : "", uiFaceName(t), uiFaceName(u), p->bad, p->badAt, p->cmds, okA, p->finished, p->sad, uiScrollActive());
        fails++;
      }
    }
  uiScrollHw = nullptr;
  uiBlit = nullptr;
  int n = 2 * THEME_COUNT;
  if (!fails) printf("hardware scroll change: ok (%d changes, both orientations); on average %ld scroll commands and %ld KB over SPI per change; drawing CPU %ld.%ld times a full drawing\n", n, cmds / n, bytes / n / 1024, (long)(tSlide / tFull * 10) / 10, (long)(tSlide / tFull * 10) % 10);
  delete p; delete a; delete b;
  return fails;
}

// Screen change: each face -> Pomodoro and Pomodoro -> each face must equal drawing
// the new screen on a clean screen. Up to 1.19 the Pomodoro and the 11th face had the same screen number and
// switching between them didn't clear: one appeared on top of the other.
static int checkTransitions(const UiState &pomo) {
  int fails = 0;
  Frame *a = new Frame(), *b = new Frame();
  for (int t = 0; t < THEME_COUNT; t++) {
    UiState c = base(14, 58, 50);
    c.theme = t;
    for (int dir = 0; dir < 2; dir++) {  // 0: face -> Pomodoro; 1: Pomodoro -> face
      uiBegin(a);
      if (dir == 0) { uiRenderClock(c); uiRenderPomodoro(pomo); } else { uiRenderPomodoro(pomo); uiRenderClock(c); }
      uiBegin(b);
      if (dir == 0) uiRenderPomodoro(pomo); else uiRenderClock(c);
      int diff = 0;
      for (int i = 0; i < 240 * 240; i++) diff += a->px[i] != b->px[i];
      if (diff) {
        printf("change %-22s %s: %d pixels left over from the previous screen\n", uiFaceName(t), dir ? "<- Pomodoro" : "-> Pomodoro", diff);
        fails++;
      }
    }
  }
  if (!fails) printf("face <-> Pomodoro change: ok (%d faces, both directions)\n", THEME_COUNT);
  delete a;
  delete b;
  return fails;
}

// Incremental sequence vs. drawing from scratch
static int checkIncremental(const char *name, UiState s, bool pomo) {
  Frame *inc = new Frame();
  uiBegin(inc);
  std::vector<uint32_t> log;
  std::vector<uint16_t> before(240 * 240);
  long flicker = 0;
  for (int step = 0; step < 140; step++) {
    memcpy(before.data(), inc->px, sizeof inc->px);
    log.clear();
    inc->log = step ? &log : nullptr;  // the first drawing is the whole screen: it doesn't count
    if (pomo) {
      s.remainingMs = s.remainingMs > 1000 ? s.remainingMs - 1000 : 0;
      s.ms += 100;
      if (step == 20) s.celebrate = true;          // phase finished: end animation
      if (step == 47) s.celebrate = false;
      if (step == 55) s.activity = ACT_CODE;       // activity change in the middle of a focus session
      if (step == 70) s.running = !s.running;      // pause in the middle
      if (step == 90) s.running = !s.running;
      if (step == 100) { s.cycleDone++; }          // cycle moves on
    } else {
      if (s.theme == THEME_PACK) s.ms += 100;  // animated sprites in packs advance with the clock in ms
      if (++s.now.tm_sec >= 60) { s.now.tm_sec = 0; if (++s.now.tm_min >= 60) { s.now.tm_min = 0; s.now.tm_hour = (s.now.tm_hour + 1) % 24; } }
      if (step == 50) { s.wx.t = 9.6f; s.wx.code = 63; s.wx.tmin = 7; s.wx.tmax = 12; s.wx.version++; }
      if (step == 80) s.today++;
      if (step == 110) { s.weatherOn = false; }
      if (step == 125) s.today = s.dailyGoal;      // goal reached: cheering tomato
    }
    if (pomo) uiRenderPomodoro(s);
    else uiRenderClock(s);
    std::set<uint32_t> flash;
    for (uint32_t e : log) {
      uint32_t i = e >> 16;
      uint16_t c = e & 0xFFFF;
      if (c != before[i] && c != inc->px[i]) flash.insert(i);
    }
    flicker += flash.size();
  }
  inc->log = nullptr;
  Frame *fresh = new Frame();
  uiBegin(fresh);
  if (pomo) uiRenderPomodoro(s);
  else uiRenderClock(s);
  int diff = 0, x0 = 240, y0 = 240, x1 = -1, y1 = -1;
  for (int i = 0; i < 240 * 240; i++)
    if (inc->px[i] != fresh->px[i]) {
      diff++;
      int x = i % 240, y = i / 240;
      if (x < x0) x0 = x; if (y < y0) y0 = y; if (x > x1) x1 = x; if (y > y1) y1 = y;
    }
  printf("%-22s %s (%d different pixels, %ld flickering pixels)", name, diff ? "FAILED" : "ok", diff, flicker);
  if (diff) printf("  area x %d-%d, y %d-%d", x0, x1, y0, y1);
  printf("\n");
  char n[64];
  snprintf(n, sizeof n, "inc_%s", name);
  save(*inc, n);
  snprintf(n, sizeof n, "fresh_%s", name);
  save(*fresh, n);
  delete inc;
  delete fresh;
  return diff;
}

// Face packs (faces/*.json at the repository root): preview in out/pack-<id>.raw and
// the same redraw test as the built-in faces.
static int testPackages(const UiState &pomo) {
  int fails = 0, packs = 0;
  DIR *dir = opendir("../faces");
  struct dirent *e;
  while (dir && (e = readdir(dir))) {
    size_t n = strlen(e->d_name);
    if (n < 6 || strcmp(e->d_name + n - 5, ".json") || !strcmp(e->d_name, "catalog.json")) continue;
    char path[160];
    snprintf(path, sizeof path, "../faces/%s", e->d_name);
    FILE *fp = fopen(path, "rb");
    if (!fp) continue;
    static char buf[16384];
    size_t len = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[len] = 0;
    char err[96];
    if (!facePackLoad(buf, len, err, sizeof err)) {
      printf("%-22s ERRO: %s\n", e->d_name, err);
      fails++;
      continue;
    }
    char name[48];
    snprintf(name, sizeof name, "pack-%s", facePackId());
    UiState st = base(14, 37, 24);
    st.theme = THEME_PACK;
    shot(name, st, false);
    if (strstr(buf, "\"frames\"") || strstr(buf, "\"glow\"")) {  // animated sprite: a few animation frames to check
      for (int k = 1; k <= 3; k++) {
        char fn[64];
        UiState a = st;
        a.ms = k * (strstr(buf, "\"glow\"") ? 600 : 170);
        snprintf(fn, sizeof fn, "%s-q%d", name, k);
        shot(fn, a, false);
      }
    }
    UiState t = base(14, 58, 50);
    t.theme = THEME_PACK;
    fails += checkIncremental(name, t, false) != 0;
    facePackUnload();
    packs++;
  }
  if (dir) closedir(dir);
  if (!packs) {  // an unreadable folder must not pass as "no packs, no error"
    printf("packs: ERROR, none read from ../faces\n");
    fails++;
  }
  (void)pomo;
  return fails;
}


// Animated sprite limits: every invalid JSON must be refused with a message; valid ones accepted
static int testSpriteLimits() {
  int fails = 0;
  auto frames = [](int n, int w, int h) {
    std::string r = "[";
    for (int k = 0; k < n; k++) {
      r += k ? ",[" : "[";
      for (int y = 0; y < h; y++) r += (y ? ",\"" : "\"") + std::string(w, k & 1 ? 'a' : 'b') + "\"";
      r += "]";
    }
    return r + "]";
  };
  auto pack = [&](const std::string &spr) {
    return "{\"format\":1,\"sprites\":{" + spr + "},\"layers\":[{\"type\":\"rect\",\"w\":10,\"h\":10}]}";
  };
  auto sp = [&](const char *name, const std::string &fr, const char *extra = "") {
    return std::string("\"") + name + "\":{\"pal\":{\"a\":\"#fff\",\"b\":\"#f00\"},\"frames\":" + fr + extra + "}";
  };
  struct { const char *nome; std::string json; bool ok; } T[] = {
    {"8 frames 32x32", pack(sp("s", frames(8, 32, 32))), true},
    {"9 frames", pack(sp("s", frames(9, 8, 8))), false},
    {"fps 11", pack(sp("s", frames(2, 8, 8), ",\"fps\":11")), false},
    {"fps 0", pack(sp("s", frames(2, 8, 8), ",\"fps\":0")), false},
    {"fps 10", pack(sp("s", frames(2, 8, 8), ",\"fps\":10")), true},
    {"different width", pack(sp("s", "[[\"aa\",\"bb\"],[\"aaa\",\"bbb\"]]")), false},
    {"different height", pack(sp("s", "[[\"aa\",\"bb\"],[\"aa\"]]")), false},
    {"33x33", pack(sp("s", frames(2, 33, 33))), false},
    {"total > 6144 bytes", pack(sp("a", frames(8, 32, 32)) + "," + sp("b", frames(8, 32, 32))), false},
    {"empty frames", pack(sp("s", "[]")), false},
  };
  for (auto &t : T) {
    char err[96] = "";
    bool ok = facePackLoad((char *)t.json.c_str(), t.json.size(), err, sizeof err);
    if (ok) facePackUnload();
    if (ok != t.ok) { printf("sprite %-20s FAILED (%s)\n", t.nome, ok ? "accepted" : err); fails++; }
    else printf("sprite %-20s ok%s%s\n",t.nome, ok ? "" : ": ", ok ? "" : err);
  }
  return fails;
}

int main() {
  uiSetLang(1);  // weatherbox: the official previews are in English
  if (const char *l = getenv("SIM_LANG")) uiSetLang(atoi(l));  // 0 = Portuguese, 1 = English, 2 = Spanish
  // ---- clock: themes ----
  UiState s = base(14, 37, 25);
  s.theme = THEME_MATRIX;              shot("01_matrix", s, false);
  { UiState t = s; t.weatherOn = false; t.today = 8; t.accent = uiRgb(255, 176, 60); shot("02_matrix_no_weather_goal", t, false); }
  s.theme = THEME_TOMATO;              shot("03_scene_day", s, false);
  { UiState t = base(23, 41, 0); t.theme = THEME_TOMATO; t.wx.code = 0; t.wx.day = false; t.wx.t = 17; shot("04_scene_night", t, false); }
  { UiState t = base(17, 52, 0); t.theme = THEME_TOMATO; t.wx.code = 63; t.wx.t = 19; shot("05_scene_rain_dusk", t, false); }
  { UiState t = base(10, 9, 0); t.theme = THEME_TOMATO; t.wx.code = 71; t.wx.t = -2; shot("05b_scene_snow", t, false); }
  { UiState t = base(13, 15, 0); t.theme = THEME_TOMATO; t.wx.code = 0; t.wx.t = 33; shot("05c_scene_hot", t, false); }
  { UiState t = base(15, 40, 0); t.theme = THEME_TOMATO; t.wx.code = 1; t.today = 8; shot("05d_scene_goal", t, false); }
  { UiState t = base(10, 9, 0); t.theme = THEME_ANALOG; shot("06_analog", t, false); }
  { UiState t = base(8, 5, 0); t.theme = THEME_WEATHER; t.wx.code = 61; t.wx.t = 18.2f; shot("07_weather_rain", t, false); }
  { UiState t = base(13, 0, 0); t.theme = THEME_WEATHER; t.wx.code = 0; t.wx.t = 31; t.wx.hum = 38; shot("08_weather_sun", t, false); }
  // °F (the firmware converts in makeState: here the values are already in °F) and rain on the way
  auto fahr = [](UiState t) {
    t.fahr = true;
    t.wx.t = t.wx.t * 9 / 5 + 32; t.wx.tmin = t.wx.tmin * 9 / 5 + 32; t.wx.tmax = t.wx.tmax * 9 / 5 + 32;
    for (UiDay &d : t.wx.d) { d.tmin = d.tmin * 9 / 5 + 32; d.tmax = d.tmax * 9 / 5 + 32; }
    return t;
  };
  { UiState t = base(13, 0, 0); t.theme = THEME_WEATHER; t.wx.code = 0; t.wx.t = 40; t.wx.tmax = 41; shot("08b_weather_fahrenheit_hot", fahr(t), false); }
  { UiState t = base(16, 20, 0); t.theme = THEME_WEATHER; t.wx.code = 3; t.wx.rainIn = 15; shot("08c_weather_rain_coming", t, false); }
  { UiState t = base(16, 20, 0); t.theme = THEME_TOMATO; t.wx.code = 3; t.wx.rainIn = 30; shot("05e_scene_rain_coming", t, false); }
  { UiState t = base(18, 26, 43); t.theme = THEME_LCD; t.wx.t = 39; shot("16b_lcd_fahrenheit", fahr(t), false); }
  { UiState t = base(9, 0, 0); t.theme = THEME_FORECAST_LCD; t.wx.d[0].tmax = 41; shot("16c_forecast_lcd_fahrenheit", fahr(t), false); }
  { UiState t = base(9, 0, 0); t.theme = THEME_FORECAST_CARDS; t.wx.d[0].tmax = 41; shot("16d_forecast_cards_fahrenheit", fahr(t), false); }

  { UiState t = base(14, 37, 24); t.theme = THEME_NEON; shot("14_neon", t, false); }
  { UiState t = base(9, 5, 0); t.theme = THEME_FLIP; shot("15_flip", t, false); }
  { UiState t = base(18, 26, 43); t.theme = THEME_LCD; shot("16_lcd", t, false); }
  { UiState t = base(14, 31, 0); t.theme = THEME_WORDS; shot("17_words_half", t, false); }
  { UiState t = base(2, 46, 0); t.theme = THEME_WORDS; shot("18_words_to", t, false); }
  { UiState t = base(11, 58, 0); t.theme = THEME_WORDS; shot("19_words_noon", t, false); }
  { UiState t = base(8, 23, 0); t.theme = THEME_WORDS; shot("20_words_morning", t, false); }

  // ---- Flip turnover animation: 14:37 -> 14:38 ----
  {
    Frame *f = new Frame();
    uiBegin(f);
    UiState t = base(14, 37, 58);
    t.theme = THEME_FLIP;
    uiRenderClock(t);
    animFrame = f;
    uiFrameHook = saveAnimFrame;
    t.now.tm_min = 38;
    uiRenderClock(t);
    uiFrameHook = nullptr;
    delete f;
  }

  // weatherbox: frames of the animated Wind face (SIM_WIND_FRAMES=n), 120 ms apart
  if (const char *nf = getenv("SIM_WIND_FRAMES")) {
    Frame *f = new Frame();
    uiBegin(f);
    UiState w = base(14, 37, 24);
    w.theme = getenv("SIM_ANIM_THEME") ? atoi(getenv("SIM_ANIM_THEME")) : THEME_WIND;
    w.wx.gust = 46;  // gusts well above the average: the arrow sways
    for (int i = 0, n = atoi(nf); i < n; i++) {
      w.ms = 1000 + i * 120;
      uiRenderClock(w);
      char name[32];
      snprintf(name, sizeof name, "wind-%02d", i);
      save(*f, name);
    }
    delete f;
  }

  // ---- panel gallery previews (docs/faces/face-N.png) ----
  for (int t = 0; t < THEME_COUNT; t++) {
    UiState g = t == THEME_ANALOG ? base(10, 9, 30) : base(14, 37, 24);
    g.theme = t;
    char n[16];
    snprintf(n, sizeof n, "face-%d", t);
    shot(n, g, false);
  }

  // ---- Pomodoro ----
  UiState p = base(14, 37, 25);
  p.phase = PH_WORK; p.running = true; p.totalMs = 25 * 60000; p.remainingMs = 10 * 60000 + 12000; p.cycleDone = 2;
  shot("09_pomo_focus", p, true);
  { UiState t = p; t.phase = PH_SHORT; t.totalMs = 5 * 60000; t.remainingMs = 3 * 60000 + 40000; t.running = false; t.now.tm_sec = 0; shot("10_pomo_break_paused", t, true); }
  { UiState t = p; t.phase = PH_LONG; t.totalMs = 15 * 60000; t.remainingMs = 14 * 60000; t.cycleDone = 4; shot("11_pomo_long_break", t, true); }
  { UiState t = p; t.phase = PH_SHORT; t.totalMs = t.remainingMs = 5 * 60000; t.cycleDone = 3; t.celebrate = true; t.ms = 3 * 160; shot("11b_pomo_end", t, true); }
  for (int a = 0; a < ACT_COUNT; a++) {  // one screen per focus activity
    UiState t = p;
    t.activity = a;
    char n[32];
    snprintf(n, sizeof n, "11c_pomo_atividade_%d", a);
    shot(n, t, true);
  }

  // ---- one-off screens ----
  { Frame *f = new Frame(); uiBegin(f); uiMessage("Set up Wi-Fi", "WeatherClock-Setup", "password: ab12cd34", "http://192.168.4.1"); save(*f, "12_portal"); delete f; }
  { Frame *f = new Frame(); uiBegin(f);  // start-up screen: the Wi-Fi step, the time step and ready
    if (const char *o = getenv("SIM_OWNER")) uiSetOwner(o);  // weatherbox: the clock's name on the start-up screen
    uiMessage("Weather Clock", "Connecting Wi-Fi", "v1.0", ""); uiBootProgress(300, 0); save(*f, "12b_start_wifi");
    uiMessage("Weather Clock", "Setting the time", "weatherclock.local", "192.168.1.178"); uiBootProgress(900, 1); save(*f, "12c_start_time");
    uiMessage("Weather Clock", "Connected", "weatherclock.local", "192.168.1.178"); uiBootProgress(900, 2); save(*f, "12d_start_ready"); delete f; }
  { Frame *f = new Frame(); uiBegin(f);  // no time yet: the face with the "syncing" indicator
    UiState t = base(14, 58, 50); t.timeOk = false; t.theme = 0; uiRenderClock(t); uiSyncHint(400); save(*f, "12e_no_time"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiSetupWifi("WeatherClock-Setup", "ab12cd34", true); save(*f, "14c_setup_wifi_qr"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiSetupCity("192.168.1.44", "weatherclock", true); save(*f, "14d_setup_city_qr"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiSetupWifi("WeatherClock-Setup", "ab12cd34"); save(*f, "14_setup_wifi"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiSetupCity("192.168.1.178", "weatherclock"); save(*f, "14b_setup_city"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiColorTest(); save(*f, "13_colour_test"); delete f; }
  { Frame *f = new Frame(); uiBegin(f); uiVcomTest(0x20, true); uiVcomTest(0x1C, false); save(*f, "13b_anti_ghosting"); delete f; }

  assetSheet();

  // ---- incremental redraw ----
  int fails = 0;
  for (int t = 0; t < THEME_COUNT; t++) {
    UiState st = base(14, 58, 50);
    st.theme = t;
    // file name from the registry (faces_list.h), without accents or spaces
    char slug[32];
    int k = 0;
    for (const unsigned char *p = (const unsigned char *)uiFaceName(t); *p && k < 30; p++) {
      if (*p == 0xC3 && p[1]) {  // á â ã à é ê í ó ô õ ú ç in UTF-8
        unsigned char c = *++p;
        slug[k++] = c >= 0xA0 && c <= 0xA5 ? 'a' : c == 0xA7 ? 'c' : c >= 0xA8 && c <= 0xAB ? 'e' : c >= 0xAC && c <= 0xAF ? 'i' : c >= 0xB2 && c <= 0xB6 ? 'o' : c >= 0xB9 && c <= 0xBC ? 'u' : '_';
      } else slug[k++] = *p == ' ' ? '_' : (*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
    }
    slug[k] = 0;
    fails += checkIncremental(slug, st, false) != 0;
  }
  fails += checkIncremental("pomodoro", p, true) != 0;
  fails += checkTransitions(p);
  fails += checkSlide();
  fails += testPackages(p);
  fails += testSpriteLimits();
  return fails;
}
