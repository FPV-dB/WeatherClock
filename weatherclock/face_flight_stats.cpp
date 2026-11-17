// South Australian airport movements observed by the local ADSB.lol collector.
// Counts are estimates, and are not backfilled before the collector started.
#include "ui_kit.h"

static const uint16_t OFF_C = uiRgb(255, 205, 94);
static const uint16_t ON_C = uiRgb(111, 219, 172);

static void paintFlightStats(void *ctx) {
  const UiState &s = *(const UiState *)ctx;
  const UiFlightStats *f = s.flights;
  G->fillRect(0, 24, 240, 216, BG);
  G->fillRect(0, 28, 240, 25, uiRgb(31, 83, 123));
  text(10, 34, 2, TEXT, "AIRPORT TRAFFIC");
  if (!f || !f->date[0]) {
    textC(120, 105, 1, MUTED, "Waiting for ADSB.lol data");
    return;
  }
  char n[12];
  snprintf(n, sizeof n, "%u", f->off);
  vecText(VF_OUTFIT, 12, 65, 35, n, OFF_C, BG);
  snprintf(n, sizeof n, "%u", f->on);
  vecText(VF_OUTFIT, 131, 65, 35, n, ON_C, BG);
  text(12, 103, 1, OFF_C, "TAKEOFFS");
  text(131, 103, 1, ON_C, "LANDINGS");
  G->drawFastHLine(8, 119, 224, CARD2);
  text(10, 126, 1, MUTED, "AIRPORT");
  text(130, 126, 1, MUTED, "OFF");
  text(188, 126, 1, MUTED, "ON");
  if (!f->n) text(10, 151, 1, MUTED, "No observed movements yet");
  const int pages = (f->n + 3) / 4;
  const int page = pages ? (s.ms / 3000) % pages : 0;
  for (int i = page * 4, y = 147; i < f->n && i < page * 4 + 4; i++, y += 19) {
    text(10, y, 1, TEXT, f->rows[i].code);
    snprintf(n, sizeof n, "%u", f->rows[i].off);
    text(130, y, 1, OFF_C, n);
    snprintf(n, sizeof n, "%u", f->rows[i].on);
    text(188, y, 1, ON_C, n);
  }
  text(10, 226, 1, MUTED, f->partial ? "PARTIAL DAY - OBSERVED" : "OBSERVED SINCE MIDNIGHT");
}

void themeFlightStats(const UiState &s, bool full) {
  static uint16_t lastVersion = 0xFFFF;
  static int lastPage = -1;
  const UiFlightStats *f = s.flights;
  const int page = f && f->n ? (s.ms / 3000) % ((f->n + 3) / 4) : 0;
  const uint16_t version = f ? f->version : 0;
  if (full || version != lastVersion || page != lastPage) {
    lastVersion = version;
    lastPage = page;
    uiPaint(full, 0, 24, 240, 216, paintFlightStats, (void *)&s);
  }
}
