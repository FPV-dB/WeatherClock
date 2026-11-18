// weatherbox: AI Usage face. Claude and OpenAI Codex plan limits (5-hour and 7-day windows): how much is left,
// a bar, and how long until each window resets. Sent by the computer program (POST /api/aiusage).
// Numbers in Outfit, words in the pixel font.
#include "ui_kit.h"

static const uint16_t AI_CLAUDE = uiRgb(217, 119, 87);
static const uint16_t AI_CODEX = uiRgb(16, 163, 127);
static const uint16_t AI_LOW = uiRgb(255, 90, 71);

// "2h 5m", "1d 22h", "45m", "now"
static void untilText(char *out, size_t n, uint32_t reset, uint32_t utc) {
  if (!reset || !utc) { snprintf(out, n, "?"); return; }
  if (reset <= utc) { snprintf(out, n, "now"); return; }
  const uint32_t m = (reset - utc + 59) / 60;
  if (m >= 24 * 60) snprintf(out, n, "%ud %uh", (unsigned)(m / 1440), (unsigned)(m % 1440 / 60));
  else if (m >= 60) snprintf(out, n, "%uh %um", (unsigned)(m / 60), (unsigned)(m % 60));
  else snprintf(out, n, "%um", (unsigned)m);
}

// One window: "5 HOURS  88% left", the bar, "Resets in 2h 5m"
static void aiRow(int y, const char *label, uint8_t left, uint32_t reset, uint32_t utc, uint16_t c) {
  text(14, y + 2, 1, TEXT, label);
  char n[8];
  snprintf(n, sizeof n, "%u%%", (unsigned)left);
  const uint16_t nc = left <= 10 ? AI_LOW : TEXT;
  const int lw = textW(" left", 1);
  const int nw = vecWidth(VF_OUTFIT_TXT, n, 13);
  vecText(VF_OUTFIT_TXT, 226 - lw - nw, y - 1, 13, n, nc, CARD);
  text(226 - lw, y + 2, 1, MUTED, " left");
  G->fillRoundRect(14, y + 14, 212, 6, 3, CARD2);
  const int w = left >= 100 ? 212 : left * 212 / 100;
  if (w >= 6) G->fillRoundRect(14, y + 14, w, 6, 3, c);
  else if (w > 0) G->fillRect(14, y + 14, w, 6, c);
  char u[20], l[32];
  untilText(u, sizeof u, reset, utc);
  snprintf(l, sizeof l, "Resets in %s", u);
  text(14, y + 24, 1, MUTED, l);
}

static void aiCard(int y, const char *title, uint16_t c, const UiAiUsage::Svc &v, uint32_t utc) {
  G->fillRoundRect(6, y, 228, 89, 8, CARD);
  text(14, y + 6, 1, c, title);
  if (!v.ok) {
    textC(120, y + 36, 1, MUTED, "No data yet");
    textC(120, y + 50, 1, MUTED, "needs the Mac program");
    return;
  }
  aiRow(y + 19, "5 HOURS", v.left5, v.reset5, utc, c);
  aiRow(y + 54, "7 DAYS", v.left7, v.reset7, utc, c);
}

static void paintAiHead(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 0, 240, 58, BG);
  char hm[8] = "--:--";
  if (s.timeOk) snprintf(hm, sizeof hm, "%02d:%02d", displayHour(s), s.now.tm_min);
  vecText(VF_OUTFIT, 120 - vecWidth(VF_OUTFIT, hm, 30) / 2, 1, 30, hm, s.accent, BG);
  if (s.weatherOn && s.wx.ok) {
    // "23°  44% humidity" (numbers in Outfit, words in the pixel font), centred
    char t[8], h[8];
    snprintf(t, sizeof t, "%d", (int)lroundf(s.wx.t));
    snprintf(h, sizeof h, "%d%%", s.wx.hum);
    const int tw = vecWidth(VF_OUTFIT_TXT, t, 12), hw = vecWidth(VF_OUTFIT_TXT, h, 12);
    const int ww = textW(" humidity", 1);
    const int total = tw + 6 + 12 + hw + ww;
    int x = 120 - total / 2;
    vecText(VF_OUTFIT_TXT, x, 34, 12, t, MUTED, BG);
    degree(x + tw + 1, 35, 1, MUTED);
    x += tw + 6 + 12;
    vecText(VF_OUTFIT_TXT, x, 34, 12, h, MUTED, BG);
    text(x + hw, 37, 1, MUTED, " humidity");
  }
  if (s.city) {
    char city[28];
    snprintf(city, sizeof city, "%.26s", s.city);
    textC(120, 48, 1, MUTED, city);
  }
}

static void paintAiBody(void *c) {
  const UiState &s = *(const UiState *)c;
  G->fillRect(0, 58, 240, 182, BG);
  static const UiAiUsage none = {};
  const UiAiUsage &a = s.ai ? *s.ai : none;
  aiCard(58, "CLAUDE", AI_CLAUDE, a.svc[0], s.utc);
  aiCard(150, "OPENAI CODEX", AI_CODEX, a.svc[1], s.utc);
}

void themeAiUsage(const UiState &s, bool full) {
  static int lastMinute = -1;
  static uint16_t lastV = 0xFFFF;
  static float lastT = -999;
  static int lastHum = -1;
  const int minute = s.timeOk ? s.now.tm_hour * 60 + s.now.tm_min : -1;
  const uint16_t v = s.ai ? s.ai->version : 0;
  const float t = s.weatherOn && s.wx.ok ? s.wx.t : -999;
  const int hum = s.weatherOn && s.wx.ok ? s.wx.hum : -1;
  const bool tick = full || minute != lastMinute;
  if (tick || t != lastT || hum != lastHum) uiPaint(full, 0, 0, 240, 58, paintAiHead, (void *)&s);
  if (tick || v != lastV) uiPaint(full, 0, 58, 240, 182, paintAiBody, (void *)&s);
  lastMinute = minute;
  lastV = v;
  lastT = t;
  lastHum = hum;
}
