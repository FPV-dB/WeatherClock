// Builds the real history functions (/stats.bin) from main.cpp with an in-memory LittleFS that mimics the
// ESP8266 core's (seek past the end fails, "w"/"w+" truncate, "r"/"r+" require the file) and checks that the
// history survives restarts, that the ring wraps around and that the short file of old versions is
// recovered. Usage: node sim/stats_test.cjs out.cpp [main.cpp]; then g++ -std=c++17 out.cpp.
const fs = require('fs'), path = require('path');
const source = fs.readFileSync(process.argv[3] || path.join(__dirname, '../weatherclock/main.cpp'), 'utf8');
function fn(sig) { const a = source.indexOf(sig), b = source.indexOf('\n}', a); if (a < 0 || b < 0) throw Error(sig); return source.slice(a, b + 2); }
const out = process.argv[2];
if (!out) throw Error('output.cpp required');
fs.writeFileSync(out, `#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
using std::min;
static std::map<std::string, std::vector<uint8_t>> disk;
struct File {
  std::vector<uint8_t> *d = nullptr; size_t pos = 0;
  operator bool() const { return d; }
  size_t size() const { return d->size(); }
  bool seek(size_t p) { if (p > d->size()) return false; pos = p; return true; }  // like the core's LittleFS
  size_t write(const uint8_t *b, size_t n) { if (pos + n > d->size()) d->resize(pos + n); memcpy(d->data() + pos, b, n); pos += n; return n; }
  size_t write(uint8_t b) { return write(&b, 1); }
  size_t read(uint8_t *b, size_t n) { n = min(n, d->size() - pos); memcpy(b, d->data() + pos, n); pos += n; return n; }
  void close() { d = nullptr; }
};
struct { File open(const char *n, const char *m) {
  File f; std::string s = m;
  if (s[0] == 'w') disk[n].clear();
  else if (!disk.count(n)) return f;
  f.d = &disk[n]; return f; } } LittleFS;
static bool fsOk = true;
struct { uint8_t dailyGoal = 2; } cfg;
${source.match(/struct StatsHead [^\n]*\n/)[0]}${source.match(/const uint32_t STATS_MAGIC[^\n]*\n/)[0]}${source.match(/const uint16_t STATS_DAYS[^\n]*\n/)[0]}StatsHead stats = {0, 0, 0, 0};
uint16_t statsStreak = 0, statsBest = 0;
static bool writerBusy = false;
bool statsWritePending = false;
uint16_t statsPendingDay = 0;
uint8_t statsPendingCount = 0;
bool fsWriterBusy() { return writerBusy; }
${fn('bool statsRead(')}
${fn('void statsCompute(')}
${fn('void statsWrite(')}
${fn('void statsFlush() {')}
${fn('void statsLoad(')}
static void boot() { stats = {0, 0, 0, 0}; statsLoad(); }
static int dayCount(uint16_t day) { uint8_t d[STATS_DAYS]; if (!statsRead(d)) return -1; return d[STATS_DAYS - 1 - (stats.lastDay - day)]; }
int main() {
  const uint16_t D = 20749;
  // day by day, with a restart (update) between each day
  boot();
  for (int i = 0; i < 10; i++) { statsWrite(D + i, 3); stats.total += 3; statsWrite(D + i, 3); boot(); }
  assert(disk["/stats.bin"].size() == sizeof(StatsHead) + STATS_DAYS);
  assert(stats.magic == STATS_MAGIC && stats.lastDay == D + 9);
  for (int i = 0; i < 10; i++) assert(dayCount(D + i) == 3);
  assert(statsStreak == 10);
  // days switched off become zero; the ring wraps around without losing the recent ones
  statsWrite(D + 12, 1); boot();
  assert(dayCount(D + 10) == 0 && dayCount(D + 11) == 0 && dayCount(D + 12) == 1 && dayCount(D + 9) == 3);
  for (int i = 13; i < 400; i++) statsWrite(D + i, i % 5);
  boot();
  assert(dayCount(D + 399) == 399 % 5 && dayCount(D + 300) == 300 % 5);
  // time in the past (forged NTP): nothing is erased or written (1.19.1+; before, it wiped the history)
  statsWrite(D + 100, 9); boot();
  assert(dayCount(D + 399) == 399 % 5 && stats.lastDay == D + 399);
  // storage formatted with the clock on: writes again (before, "r+" failed until a restart)
  disk.clear(); statsWrite(D + 400, 2); boot();
  assert(dayCount(D + 400) == 2);
  // short file from old versions (371 bytes): recovered at boot and rewritten with 383
  disk.clear();
  std::vector<uint8_t> old(STATS_DAYS, 0);
  StatsHead h = {STATS_MAGIC, D, 4, 7};
  memcpy(old.data(), &h, sizeof h);
  old[sizeof(StatsHead) + D % STATS_DAYS] = 5;
  disk["/stats.bin"] = old;
  boot();
  assert(disk["/stats.bin"].size() == sizeof(StatsHead) + STATS_DAYS);
  assert(stats.magic == STATS_MAGIC && stats.total == 7 && dayCount(D) == 5);
  // an upload keeps another file open for writing: the history waits and persists on close
  std::vector<uint8_t> before = disk["/stats.bin"];
  writerBusy = true;
  stats.total++;
  statsWrite(D + 1, 6);
  assert(statsWritePending && statsPendingDay == D + 1 && statsPendingCount == 6);
  assert(disk["/stats.bin"] == before);
  writerBusy = false;
  statsFlush();
  assert(!statsWritePending && disk["/stats.bin"] != before);
  boot();
  assert(stats.total == 8 && dayCount(D + 1) == 6);
  puts("stats: history survives restarts, the ring and formatting; writes wait for the upload to finish");
}
`);
