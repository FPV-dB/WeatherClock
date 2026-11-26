// weatherbox: Aircraft Bad Squawks face. The last 20 aircraft seen anywhere squawking 7700 (emergency),
// 7500 (hijack), 7600 (radio failure) or 0100, newest first, five to a page: the code, callsign, type,
// registration, route or operator, where it was, its altitude and how long ago. Pages turn every 2.5 s,
// so all four fit in one 10 s rotation slot. Sent by the computer program (POST /api/squawks).
// Numbers in Outfit, words in the pixel font. Drawn off-screen (no flicker).
#include "ui_kit.h"

static const uint16_t SQ_BAND = uiRgb(150, 24, 24);
static const uint16_t SQ_LINE = uiRgb(215, 215, 222);
static const int SQ_ROWS = 5;          // rows per page
static const uint32_t SQ_PAGE_MS = 2500;
static const int SQ_TOP = 48, SQ_ROW_H = 37;

static uint16_t codeColor(const char *c) {
  if (!strcmp(c, "7700")) return uiRgb(255, 70, 60);    // emergency
  if (!strcmp(c, "7500")) return uiRgb(220, 90, 255);   // hijack
  if (!strcmp(c, "7600")) return uiRgb(255, 170, 40);   // radio failure
  return uiRgb(90, 170, 255);                           // 0100 and anything else
}
static const char *codeLabel(const char *c) {
  if (!strcmp(c, "7700")) return "EMERG";
  if (!strcmp(c, "7500")) return "HIJACK";
  if (!strcmp(c, "7600")) return "RADIO";
  return "CODE";
}

// "now", "12m ago", "3h ago", "2d ago"
static void agoText(char *out, size_t n, const UiSquawk &q, uint32_t utc, bool fresh) {
  if (q.live && fresh) { snprintf(out, n, "NOW"); return; }
  if (!q.seen || !utc) { out[0] = 0; return; }
  const uint32_t m = utc > q.seen ? (utc - q.seen) / 60 : 0;
  if (m < 1) snprintf(out, n, "just now");
  else if (m < 60) snprintf(out, n, "%um ago", (unsigned)m);
  else if (m < 48 * 60) snprintf(out, n, "%uh ago", (unsigned)(m / 60));
  else snprintf(out, n, "%ud ago", (unsigned)(m / 1440));
}

// "FL350", "8,500 ft", "GND", ""
static void altText(char *out, size_t n, int32_t alt) {
  if (alt == -2) snprintf(out, n, "GND");
  else if (alt < 0) out[0] = 0;
  else if (alt >= 18000) snprintf(out, n, "FL%03d", (int)((alt + 50) / 100));
  else if (alt >= 1000) snprintf(out, n, "%d,%03d ft", (int)(alt / 1000), (int)(alt % 1000));
  else snprintf(out, n, "%d ft", (int)alt);
}

static void sqRow(int y, const UiSquawk &q, uint32_t utc, bool fresh) {
  // the code badge: number in Outfit, what it means underneath
  const uint16_t cc = codeColor(q.code);
  G->fillRoundRect(6, y + 2, 44, 31, 5, cc);
  vecText(VF_OUTFIT_TXT, 28 - vecWidth(VF_OUTFIT_TXT, q.code, 13) / 2, y + 4, 13, q.code, BG, cc);
  textC(28, y + 23, 1, BG, codeLabel(q.code));
  // line 1: callsign and type, then how long ago (or NOW while it is still squawking)
  char l[32], r[12];
  snprintf(l, sizeof l, "%.8s", q.call[0] ? q.call : q.reg[0] ? q.reg : "?");
  text(56, y + 3, 1, TEXT, l);
  if (q.type[0]) text(56 + textW(l, 1) + 6, y + 3, 1, MUTED, q.type);
  agoText(r, sizeof r, q, utc, fresh);
  if (r[0]) textR(234, y + 3, 1, q.live && fresh ? cc : MUTED, r);
  // line 2: registration and route (or operator)
  if (q.reg[0] && q.call[0] && strcmp(q.reg, q.call)) snprintf(l, sizeof l, "%-8s %.20s", q.reg, q.info);
  else snprintf(l, sizeof l, "%.29s", q.info);
  text(56, y + 14, 1, SQ_LINE, l);
  // line 3: where it was, and its altitude
  altText(r, sizeof r, q.alt);
  const int rw = r[0] ? textW(r, 1) : 0;
  if (q.near[0]) {
    const int room = (178 - rw - (rw ? 6 : 0)) / 6;
    snprintf(l, sizeof l, "%.*s", room > 0 ? (room < 29 ? room : 29) : 0, q.near);
    text(56, y + 25, 1, MUTED, l);
  }
  if (r[0]) textR(234, y + 25, 1, MUTED, r);
}

struct SqCtx { const UiState *s; int page, pages; };

static void paintSquawks(void *c) {
  const SqCtx &k = *(const SqCtx *)c;
  const UiState &s = *k.s;
  G->fillRect(0, 24, 240, 216, BG);
  G->fillRect(0, 26, 240, 19, SQ_BAND);
  text(8, 28, 2, TEXT, "BAD SQUAWKS");
  const UiSquawks *q = s.squawks;
  if (!q || !q->n) {
    textC(120, 100, 2, MUTED, "No data yet");
    textC(120, 126, 1, MUTED, "needs the radar program");
    textC(120, 138, 1, MUTED, "on a computer");
    return;
  }
  char h[40];
  const int first = k.page * SQ_ROWS, last = first + SQ_ROWS < q->n ? first + SQ_ROWS : q->n;
  snprintf(h, sizeof h, "%d-%d of %d", first + 1, last, q->n);
  textR(232, 32, 1, uiRgb(255, 215, 210), h);
  const bool fresh = s.ms - q->at < 10 * 60000UL;  // "NOW" only while the computer program keeps sending
  for (int i = first; i < last; i++) {
    const int y = SQ_TOP + (i - first) * SQ_ROW_H;
    if (i > first) G->drawFastHLine(56, y - 1, 178, CARD2);
    sqRow(y, q->rows[i], s.utc, fresh);
  }
  // page dots
  if (k.pages > 1)
    for (int p = 0; p < k.pages; p++)
      G->fillCircle(120 - (k.pages - 1) * 5 + p * 10, 236, 2, p == k.page ? TEXT : CARD2);
}

// Date on the left and the time on the right, like the Weather face.
static void sqTopBar(const UiState &s, bool full) {
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

void themeSquawks(const UiState &s, bool full) {
  sqTopBar(s, full);
  static uint32_t start = 0;
  static int lastPage = -1, lastMin = -1;
  static uint16_t lastV = 0xFFFF;
  if (full) start = s.ms;
  const int n = s.squawks ? s.squawks->n : 0;
  const int pages = n ? (n + SQ_ROWS - 1) / SQ_ROWS : 1;
  const int page = (int)((s.ms - start) / SQ_PAGE_MS % pages);
  const uint16_t v = s.squawks ? s.squawks->version : 0;
  const int minute = s.timeOk ? s.now.tm_hour * 60 + s.now.tm_min : -1;  // "12m ago" moves on
  if (full || page != lastPage || v != lastV || minute != lastMin) {
    lastPage = page;
    lastV = v;
    lastMin = minute;
    SqCtx k = {&s, page, pages};
    uiPaint(full, 0, 24, 240, 216, paintSquawks, &k);
  }
}
