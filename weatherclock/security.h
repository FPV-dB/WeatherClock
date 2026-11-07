#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
// Allocation-free checks shared with the regression tests.
struct UploadGate {
  size_t bytes = 0, limit = 0;
  bool active = false;
  void begin(bool authorized, size_t maxBytes) { bytes = 0; limit = maxBytes; active = authorized; }
  bool take(size_t n) {
    if (!active || n > limit - bytes) { active = false; return false; }
    bytes += n; return true;
  }
  void stop() { active = false; }
};
struct AuthThrottle {
  struct Entry { uint32_t ip = 0, until = 0, last = 0; uint8_t failures = 0; } entries[4];
  static bool before(uint32_t now, uint32_t deadline) { return (int32_t)(deadline - now) > 0; }
  Entry *find(uint32_t ip, uint32_t now, bool create) {
    Entry *empty = nullptr;
    Entry *oldest = &entries[0];
    for (Entry &e : entries) {
      if (e.ip == ip) return &e;
      if (!e.ip) { if (!empty) empty = &e; continue; }
      if ((int32_t)(e.last - oldest->last) < 0) oldest = &e;
    }
    if (!create) return nullptr;
    Entry *slot = empty ? empty : oldest;
    *slot = Entry();
    slot->last = now;
    return slot;
  }
  bool blocked(uint32_t ip, uint32_t now, uint32_t *waitMs = nullptr) {
    Entry *e = find(ip, now, false);
    bool yes = e && before(now, e->until);
    if (waitMs) *waitMs = yes ? e->until - now : 0;
    return yes;
  }
  uint32_t fail(uint32_t ip, uint32_t now) {
    Entry *e = find(ip, now, true);
    if (e->ip != ip || now - e->last > 300000UL) e->failures = 0;
    e->ip = ip;
    if (e->failures < 6) e->failures++;
    uint32_t wait = 1000UL << (e->failures - 1);  // 1, 2, 4, 8, 16, 32 s
    e->last = now;
    e->until = now + wait;
    return wait;
  }
  void success(uint32_t ip) {
    for (Entry &e : entries) if (e.ip == ip) { e = Entry(); return; }
  }
};
static bool safeHttpUrl(const char *s, size_t n) {
  if (n < 8 || n > 400 || memcmp(s, "http://", 7)) return false;
  for (size_t i=0; i<n; i++) {
    unsigned char c = s[i];
    if (c <= 0x20 || c == 0x7f || c == '"' || c == '\\' || c == '#') return false;
  }
  size_t end=7;
  while (end<n && s[end]!='/' && s[end]!='?') end++;
  size_t hostEnd=7;
  while(hostEnd<end && s[hostEnd]!=':') {
    char c=s[hostEnd++];
    if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return false;
  }
  if (hostEnd==7) return false;
  if (hostEnd<end) {
    uint32_t port=0;
    if (++hostEnd==end) return false;
    while(hostEnd<end) {
      char c=s[hostEnd++]; if(c<'0'||c>'9')return false;
      port=port*10+(c-'0'); if(port>65535)return false;
    }
    if (!port) return false;
  }
  return true;
}
static bool safeIpv4Host(const char *s, size_t n) {
  if (!n || n > 21) return false;
  size_t i = 0;
  for (int part = 0; part < 4; part++) {
    if (i >= n || s[i] < '0' || s[i] > '9') return false;
    unsigned value = 0, digits = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
      value = value * 10 + (unsigned)(s[i++] - '0');
      if (++digits > 3 || value > 255) return false;
    }
    if (part < 3) {
      if (i >= n || s[i++] != '.') return false;
    }
  }
  if (i == n) return true;
  if (s[i++] != ':' || i == n) return false;
  unsigned port = 0;
  while (i < n) {
    if (s[i] < '0' || s[i] > '9') return false;
    port = port * 10 + (unsigned)(s[i++] - '0');
    if (port > 65535) return false;
  }
  return port > 0;
}

// The clock's own name on the local network ("weatherclock-1a2b.local", optional port), case-
// insensitive. Only that exact name: any other name would reopen the DNS rebinding that the IPv4 check closes.
static bool safeOwnHost(const char *s, size_t n, const char *name) {
  size_t k = strlen(name);
  if (!k || n < k + 6) return false;
  for (size_t i = 0; i < k; i++)
    if (tolower((unsigned char)s[i]) != tolower((unsigned char)name[i])) return false;
  const char *dot = ".local";
  for (size_t i = 0; i < 6; i++)
    if (tolower((unsigned char)s[k + i]) != dot[i]) return false;
  size_t i = k + 6;
  if (i == n) return true;
  if (s[i++] != ':' || i == n || n - i > 5) return false;
  unsigned port = 0;
  for (; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    port = port * 10 + (unsigned)(s[i] - '0');
  }
  return port > 0 && port <= 65535;
}

// ---------- minimal mDNS (RFC 6762): only answers "what is <name>.local's IPv4?" ----------
// The core's ESP8266mDNS costs ~21 KB; to open the panel by name the A record is enough.
// Reads a DNS name at p[i..n) (compression pointers: at most 8 jumps), lowercased and with dots.
// A label containing '.' or a null byte is refused ("weatherclock-1a2b.local" in a single label doesn't become our name).
static bool dnsReadName(const uint8_t *p, size_t n, size_t &i, char *out, size_t cap) {
  size_t pos = i, len = 0;
  bool jumped = false;
  int jumps = 0;
  for (;;) {
    if (pos >= n) return false;
    uint8_t c = p[pos];
    if (c == 0) {
      pos++;
      break;
    }
    if ((c & 0xC0) == 0xC0) {
      if (pos + 1 >= n || ++jumps > 8) return false;
      if (!jumped) i = pos + 2;
      jumped = true;
      pos = ((size_t)(c & 0x3F) << 8) | p[pos + 1];
      continue;
    }
    if (c & 0xC0 || pos + 1 + c > n) return false;
    if (len) {
      if (len + 1 >= cap) return false;
      out[len++] = '.';
    }
    for (size_t k = 0; k < c; k++) {
      uint8_t ch = p[pos + 1 + k];
      if (ch == '.' || ch == 0 || len + 1 >= cap) return false;
      out[len++] = (char)tolower(ch);
    }
    pos += 1 + c;
  }
  if (!jumped) i = pos;
  out[len] = 0;
  return true;
}

// Is it a question (not an answer) for the A or ANY record, class IN, of "<name>.local"? *qu = asked for a
// direct answer (QU bit). Name compared case-insensitively.
static bool mdnsAsksFor(const uint8_t *p, size_t n, const char *name, bool *qu) {
  if (n < 12 || (p[2] & 0xF8)) return false;  // QR=1 (answer) or an opcode other than query
  unsigned qd = (unsigned)p[4] << 8 | p[5];
  if (qd > 16) return false;
  char want[40], got[64];
  if (snprintf(want, sizeof want, "%s.local", name) >= (int)sizeof want) return false;
  for (char *c = want; *c; c++) *c = (char)tolower((unsigned char)*c);
  size_t i = 12;
  for (unsigned q = 0; q < qd; q++) {
    if (!dnsReadName(p, n, i, got, sizeof got) || i + 4 > n) return false;
    unsigned type = (unsigned)p[i] << 8 | p[i + 1], cls = (unsigned)p[i + 2] << 8 | p[i + 3];
    i += 4;
    if ((type == 1 || type == 255) && (cls & 0x7FFF) == 1 && !strcmp(got, want)) {
      if (qu) *qu = cls & 0x8000;
      return true;
    }
  }
  return false;
}

// Is another device already answering for "<name>.local"? (mDNS answer with a record of that name)
static bool mdnsHasAnswer(const uint8_t *p, size_t n, const char *name) {
  if (n < 12 || !(p[2] & 0x80)) return false;  // answers only
  unsigned qd = (unsigned)p[4] << 8 | p[5], rr = ((unsigned)p[6] << 8 | p[7]) + ((unsigned)p[8] << 8 | p[9]) + ((unsigned)p[10] << 8 | p[11]);
  if (qd > 16 || rr > 64) return false;
  char want[40], got[64];
  if (snprintf(want, sizeof want, "%s.local", name) >= (int)sizeof want) return false;
  for (char *c = want; *c; c++) *c = (char)tolower((unsigned char)*c);
  size_t i = 12;
  for (unsigned q = 0; q < qd; q++) {
    if (!dnsReadName(p, n, i, got, sizeof got) || i + 4 > n) return false;
    i += 4;
  }
  for (unsigned r = 0; r < rr; r++) {
    if (!dnsReadName(p, n, i, got, sizeof got) || i + 10 > n) return false;
    if (!strcmp(got, want)) return true;
    size_t len = (size_t)p[i + 8] << 8 | p[i + 9];
    i += 10 + len;
    if (i > n) return false;
  }
  return false;
}

// Probe question (RFC 6762, 8.1): "is anyone already using <name>.local?", type ANY, asking for a direct answer.
static size_t mdnsProbeQuery(uint8_t *o, size_t cap, const char *name) {
  size_t k = strlen(name);
  if (!k || k > 63 || cap < 12 + k + 8 + 4) return 0;
  memset(o, 0, 12);
  o[5] = 1;
  size_t i = 12;
  o[i++] = (uint8_t)k;
  memcpy(o + i, name, k);
  i += k;
  o[i++] = 5;
  memcpy(o + i, "local", 5);
  i += 5;
  o[i++] = 0;
  o[i++] = 0;
  o[i++] = 255;
  o[i++] = 0x80;  // QU: answer directly
  o[i++] = 1;
  return i;
}

// Builds the answer with the A record. legacy: the asker doesn't use port 5353 (ordinary resolver):
// returns the same id, repeats the question and uses a short TTL, as RFC 6762 (6.7) asks. 0 if it doesn't fit.
static size_t mdnsAnswer(uint8_t *o, size_t cap, const char *name, const uint8_t ip[4], uint16_t id, bool legacy) {
  size_t k = strlen(name), need = 12 + (legacy ? k + 8 + 4 + 2 : k + 8) + 10 + 4;
  if (!k || k > 63 || cap < need) return 0;
  size_t i = 0;
  auto u16 = [&](unsigned v) { o[i++] = (uint8_t)(v >> 8); o[i++] = (uint8_t)v; };
  auto qname = [&] {
    o[i++] = (uint8_t)k;
    memcpy(o + i, name, k);
    i += k;
    o[i++] = 5;
    memcpy(o + i, "local", 5);
    i += 5;
    o[i++] = 0;
  };
  u16(legacy ? id : 0);
  u16(0x8400);  // authoritative answer
  u16(legacy ? 1 : 0);
  u16(1);
  u16(0);
  u16(0);
  if (legacy) {
    qname();
    u16(1);
    u16(1);
    u16(0xC00C);  // points to the question's name
  } else {
    qname();
  }
  u16(1);                         // A
  u16(legacy ? 0x0001 : 0x8001);  // IN (+ cache-flush no mDNS)
  u16(0);
  u16(legacy ? 10 : 120);  // TTL
  u16(4);
  memcpy(o + i, ip, 4);
  i += 4;
  return i;
}

// days since 1970 of a calendar date (y/m/d, m = 1..12)
static int64_t civilDays(int y, int m, int d) {
  y -= m <= 2;  // "days from civil"
  int era = y / 400, yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return (int64_t)era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

// "2026-10-23T00:30" -> seconds since 1970 (as if it were UTC; the caller removes the time zone). -1 if invalid.
static int64_t isoToEpoch(const char *s) {
  if (!s || strlen(s) < 16 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':') return -1;
  int v[5], at[5] = {0, 5, 8, 11, 14}, len[5] = {4, 2, 2, 2, 2};
  for (int k = 0; k < 5; k++) {
    v[k] = 0;
    for (int i = 0; i < len[k]; i++) {
      char c = s[at[k] + i];
      if (c < '0' || c > '9') return -1;
      v[k] = v[k] * 10 + (c - '0');
    }
  }
  int y = v[0], m = v[1], d = v[2];
  if (m < 1 || m > 12 || d < 1 || d > 31 || v[3] > 23 || v[4] > 59) return -1;
  return civilDays(y, m, d) * 86400 + v[3] * 3600 + v[4] * 60;
}

// Header "Date: Tue, 28 Oct 2026 12:00:00 GMT" (what comes after "Date:") -> seconds since 1970 (UTC).
// -1 if invalid. Only good for SHOWING the time until SNTP arrives: it comes over plain HTTP and never confirms anything.
static int64_t httpDateToEpoch(const char *s) {
  static const char MON[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  if (!s) return -1;
  while (*s == ' ') s++;
  const char *c = strchr(s, ',');
  if (c && c - s <= 3) s = c + 1;  // day of the week
  while (*s == ' ') s++;
  int d = 0, y = 0, hh, mm, ss, i = 0;
  for (; i < 2 && s[i] >= '0' && s[i] <= '9'; i++) d = d * 10 + (s[i] - '0');
  if (!i || s[i] != ' ') return -1;
  s += i + 1;
  int m = 0;
  for (int k = 0; k < 12 && !m; k++) if (!strncmp(s, MON + k * 3, 3)) m = k + 1;
  if (!m || s[3] != ' ') return -1;
  s += 4;
  for (i = 0; i < 4; i++) {
    if (s[i] < '0' || s[i] > '9') return -1;
    y = y * 10 + (s[i] - '0');
  }
  if (s[4] != ' ' || s[7] != ':' || s[10] != ':') return -1;
  int *f[3] = {&hh, &mm, &ss};
  for (int k = 0; k < 3; k++) {
    const char *p = s + 5 + k * 3;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return -1;
    *f[k] = (p[0] - '0') * 10 + (p[1] - '0');
  }
  if (d < 1 || d > 31 || y < 2000 || hh > 23 || mm > 59 || ss > 60) return -1;
  return civilDays(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
}

// Time stored in RTC memory to survive a software restart (update, settings, crash): sec is the
// time at the moment ticks (system_get_rtc_time) was read. The RTC keeps counting during the restart; cal is the
// tick period in µs (system_rtc_clock_cali_proc, Q12 format). On the way back, the time is sec + the elapsed time.
struct RtcClock { uint32_t magic, sec, ticks, chk; };
static const uint32_t RTC_CLOCK_MAGIC = 0x434C4B31;  // "CLK1"
static uint32_t rtcClockChk(const RtcClock &r) { return r.magic ^ (r.sec * 2654435761u) ^ (r.ticks * 40503u) ^ 0x5A5AC3C3u; }
static RtcClock rtcClockMake(uint32_t sec, uint32_t ticks) {
  RtcClock r = {RTC_CLOCK_MAGIC, sec, ticks, 0};
  r.chk = rtcClockChk(r);
  return r;
}
// The time now, or -1 if the record isn't valid (memory from an earlier power-up, corrupt, or stopped for more than 10 min)
static int64_t rtcClockRestore(const RtcClock &r, uint32_t nowTicks, uint32_t cal) {
  if (r.magic != RTC_CLOCK_MAGIC || r.chk != rtcClockChk(r) || r.sec < 1700000000u || !cal) return -1;
  uint64_t us = ((uint64_t)(uint32_t)(nowTicks - r.ticks) * cal) >> 12;
  if (us > 600000000ull) return -1;
  return (int64_t)r.sec + (int64_t)(us / 1000000ull);
}

// Protected time. The core's SNTP accepts any reply (SNTP_CHECK_RESPONSE 0 in the precompiled lwIP):
// an injected UDP packet (FragAttacks, or someone on the path) changes the time. The time is only "confirmed"
// when it matches a second source: the local time Open-Meteo sends over TCP (much harder to
// forge blind) or, without weather, two consecutive syncs that agree. With the time confirmed,
// an SNTP jump is undone until it's confirmed. The focus history is only written with ok() == true.
struct TimeGuard {
  int64_t ref = 0;              // last confirmed time (s since 1970)
  uint32_t refMs = 0;           // millis() at that moment
  int64_t cand = 0;             // SNTP time not yet confirmed
  uint32_t candMs = 0;
  bool confirmed = false;
  uint16_t undone = 0;          // jumps undone since power-on
  static bool near(int64_t a, int64_t b, int64_t tol) { return a - b <= tol && b - a <= tol; }
  int64_t expected(uint32_t ms) const { return ref + (int64_t)((uint32_t)(ms - refMs) / 1000); }
  int64_t candNow(uint32_t ms) const { return cand + (int64_t)((uint32_t)(ms - candMs) / 1000); }
  void confirm(int64_t t, uint32_t ms) { ref = t; refMs = ms; cand = 0; confirmed = true; }
  // SNTP just set the time to t. Returns the time that should apply: t, or the expected one if the jump was undone.
  // twoSyncs: without the second source (weather off), two syncs that agree confirm it.
  int64_t onSntp(int64_t t, uint32_t ms, bool twoSyncs) {
    if (confirmed && near(t, expected(ms), 120)) { ref = t; refMs = ms; return t; }  // just the usual correction
    if (twoSyncs && cand && near(t, candNow(ms), 120)) { confirm(t, ms); return t; }
    cand = t;
    candMs = ms;
    if (!confirmed) return t;  // right after power-on: valid on screen, without writing history until confirmed
    undone++;
    return expected(ms);
  }
  // UTC time from the weather (the start of the current quarter-hour: up to ~15 min ago). Returns the time that should apply.
  int64_t onWeather(int64_t utc, int64_t now, uint32_t ms) {
    const int64_t tol = 1200;
    if (near(now, utc + 450, tol)) { confirm(now, ms); return now; }
    if (cand && near(candNow(ms), utc + 450, tol)) { int64_t c = candNow(ms); confirm(c, ms); return c; }
    confirmed = false;  // none matches: keep showing it, but don't write history
    return now;
  }
  bool ok(int64_t now, uint32_t ms) const { return confirmed && near(now, expected(ms), 120); }
  // every hour: follows millis() (which wraps to zero every 49 days) without losing the reference
  void tick(int64_t now, uint32_t ms) { if (ok(now, ms) && (uint32_t)(ms - refMs) > 3600000) { ref = now; refMs = ms; } }
};

// At most n times per second (SSDP answers: a small request becomes an answer ~3x bigger).
struct RateLimit {
  uint32_t at = 0;
  uint8_t used = 0;
  bool take(uint32_t ms, uint8_t n) {
    if (ms - at >= 1000) { at = ms; used = 0; }
    return used < n && ++used;
  }
};
