#!/usr/bin/env python3
"""Aircraft that squawk an emergency code, for the Weather Clock's Aircraft Bad Squawks face.

Every minute it asks the ADS-B network for every aircraft in the world squawking 7700 (emergency),
7500 (hijack), 7600 (radio failure) or 0100, and keeps a list of the last 20 events, newest first. For
each one it looks up the route (adsbdb.com) and the nearest big airport (OurAirports), then writes
payload.txt, which push.sh sends to the clocks.

Internet only: this program never talks to the local network (macOS blocks background Python from it),
push.sh does that with curl. Standard library only, works with Apple's /usr/bin/python3 (3.9).
"""

import csv
import io
import json
import math
import pathlib
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
CONFIG_FILE = HERE / "config.json"
STATE_FILE = HERE / "events.json"      # the events, kept across restarts
AIRPORTS_FILE = HERE / "airports.json"  # big airports, downloaded once a month
ROUTES_FILE = HERE / "routes.json"      # callsign -> route cache
PAYLOAD_FILE = HERE / "payload.txt"

UA = {"User-Agent": "WeatherClock-squawks/1.0"}
FEEDS = [
    "https://opendata.adsb.fi/api/v2/sqk/{code}",
    "https://api.adsb.lol/v2/sqk/{code}",
]
AIRPORTS_URL = "https://davidmegginson.github.io/ourairports-data/airports.csv"
SHOW = 20


def log(*args):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), *args, flush=True)


def load_config():
    cfg = {
        "codes": ["7700", "7500", "7600", "0100"],
        "every_s": 60,
        "min_sightings": 2,   # a code must be seen on 2 checks in a row (filters one-off transponder glitches)
        "gap_min": 30,        # the same aircraft and code after this long counts as a new event
        "keep": 50,
    }
    if CONFIG_FILE.exists():
        cfg.update(json.loads(CONFIG_FILE.read_text()))
    return cfg


def get_json(url, timeout=20):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8", "replace"))


def load_json(path, default):
    try:
        return json.loads(path.read_text())
    except Exception:
        return default


def save_json(path, value):
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(value))
    tmp.replace(path)


# ---------- the feed ----------
feed_index = 0


def squawking(code):
    """Aircraft squawking `code` right now. Tries adsb.fi first, then adsb.lol."""
    global feed_index
    for k in range(len(FEEDS)):
        i = (feed_index + k) % len(FEEDS)
        url = FEEDS[i].format(code=code)
        try:
            data = get_json(url)
            ac = data.get("ac", data.get("aircraft")) or []
            if k:
                log("using", url.split("/")[2])
            feed_index = i if k else feed_index
            return [a for a in ac if str(a.get("squawk", "")) == code]
        except Exception as e:  # noqa: BLE001
            log(url.split("/")[2], "failed:", e)
    raise RuntimeError("no feed answered")


# ---------- places ----------
airports = []


def load_airports():
    """Large and medium airports with scheduled flights: (lat, lon, "City CC")."""
    global airports
    cached = load_json(AIRPORTS_FILE, None)
    if cached and time.time() - cached.get("at", 0) < 30 * 86400:
        airports = cached["list"]
        return
    try:
        req = urllib.request.Request(AIRPORTS_URL, headers=UA)
        with urllib.request.urlopen(req, timeout=60) as r:
            text = r.read().decode("utf-8", "replace")
        out = []
        for row in csv.DictReader(io.StringIO(text)):
            if row["type"] not in ("large_airport", "medium_airport") or row["scheduled_service"] != "yes":
                continue
            place = (row["municipality"] or row["name"]).split(",")[0].split("(")[0].strip()
            out.append([round(float(row["latitude_deg"]), 3), round(float(row["longitude_deg"]), 3),
                        f"{place} {row['iso_country']}"])
        airports = out
        save_json(AIRPORTS_FILE, {"at": time.time(), "list": out})
        log(len(out), "airports loaded")
    except Exception as e:  # noqa: BLE001
        log("airport list failed:", e)
        if cached:
            airports = cached["list"]


def near(lat, lon):
    if lat is None or lon is None or not airports:
        return ""
    best, bd = "", 1e9
    cl = math.cos(math.radians(lat))
    for a in airports:
        d = (a[0] - lat) ** 2 + ((a[1] - lon) * cl) ** 2
        if d < bd:
            best, bd = a[2], d
    km = math.sqrt(bd) * 111
    return best if km < 400 else f"{best} ({int(km)} km)" if km < 1500 else "Over the ocean"


# ---------- routes ----------
routes = {}


def route(callsign, operator):
    """'SYD > MEL' from adsbdb, else the airline or operator name."""
    cs = (callsign or "").strip()
    hit = routes.get(cs)
    if hit and time.time() - hit["at"] < 7 * 86400:
        return hit["info"]
    info = ""
    if cs:
        try:
            r = get_json("https://api.adsbdb.com/v0/callsign/" + urllib.parse.quote(cs), timeout=15)
            fr = (r.get("response") or {}).get("flightroute") or {}
            o = (fr.get("origin") or {}).get("iata_code") or (fr.get("origin") or {}).get("icao_code")
            d = (fr.get("destination") or {}).get("iata_code") or (fr.get("destination") or {}).get("icao_code")
            if o and d:
                info = f"{o} > {d}"
            elif (fr.get("airline") or {}).get("name"):
                info = fr["airline"]["name"]
        except urllib.error.HTTPError as e:
            if e.code != 404:
                log("adsbdb", cs, e)
        except Exception as e:  # noqa: BLE001
            log("adsbdb", cs, e)
    if not info:
        info = (operator or "").strip() or "No route known"
    routes[cs] = {"at": time.time(), "info": info}
    return info


# ---------- events ----------
def ascii(s, n):
    s = "".join(c if 32 <= ord(c) < 127 else " " for c in str(s or "")).strip()
    return s[:n]


def altitude(a):
    alt = a.get("alt_baro", a.get("alt_geom"))
    if alt == "ground":
        return -2
    try:
        return int(alt)
    except (TypeError, ValueError):
        return -1


def update(events, cfg, seen_now, now):
    """seen_now: {(hex, code): aircraft dict}. Updates and returns events (newest first)."""
    gap = cfg["gap_min"] * 60
    for (hx, code), a in seen_now.items():
        ev = next((e for e in events if e["hex"] == hx and e["code"] == code and now - e["last"] < gap), None)
        if not ev:
            ev = {"hex": hx, "code": code, "first": now, "count": 0}
            events.append(ev)
        ev["last"] = now
        ev["count"] += 1
        ev["call"] = ascii(a.get("flight"), 8) or ev.get("call", "")
        ev["reg"] = ascii(a.get("r"), 8) or ev.get("reg", "")
        ev["type"] = ascii(a.get("t"), 4) or ev.get("type", "")
        ev["alt"] = altitude(a)
        ev["op"] = ascii(a.get("ownOp"), 30) or ev.get("op", "")
        if a.get("lat") is not None:
            ev["near"] = near(a.get("lat"), a.get("lon"))
        if "info" not in ev and ev["count"] >= cfg["min_sightings"]:
            ev["info"] = route(ev["call"], ev["op"])
    # one-off sightings that never repeated are dropped once they are old
    events = [e for e in events if e["count"] >= cfg["min_sightings"] or now - e["last"] < gap]
    events.sort(key=lambda e: e["first"], reverse=True)
    return events[: cfg["keep"]]


def payload(events, cfg, now, every):
    shown = [e for e in events if e["count"] >= cfg["min_sightings"]][:SHOW]
    v = {}
    for i, e in enumerate(shown):
        v[f"q{i}"] = e["code"]
        v[f"f{i}"] = e.get("call", "")
        v[f"r{i}"] = e.get("reg", "")
        v[f"t{i}"] = e.get("type", "")
        v[f"i{i}"] = ascii(e.get("info", ""), 21)
        v[f"w{i}"] = ascii(e.get("near", ""), 21)
        v[f"a{i}"] = e.get("alt", -1)
        v[f"s{i}"] = int(e["first"])
        v[f"l{i}"] = 1 if now - e["last"] < every * 2.5 else 0
    tmp = PAYLOAD_FILE.with_suffix(".tmp")
    tmp.write_text(urllib.parse.urlencode(v))
    tmp.replace(PAYLOAD_FILE)
    return len(shown)


def main():
    global routes
    cfg = load_config()
    log("watching squawks", ", ".join(cfg["codes"]))
    routes = load_json(ROUTES_FILE, {})
    events = load_json(STATE_FILE, [])
    load_airports()
    while True:
        start = time.time()
        if time.time() - (load_json(AIRPORTS_FILE, {}).get("at", 0)) > 30 * 86400:
            load_airports()
        try:
            seen = {}
            for code in cfg["codes"]:
                for a in squawking(code):
                    hx = str(a.get("hex", "")).lower().lstrip("~")
                    if hx:
                        seen[(hx, code)] = a
                time.sleep(1.5)  # adsb.fi allows one request a second
            now = time.time()
            before = {(e["hex"], e["code"]) for e in events if e["count"] >= cfg["min_sightings"]}
            events = update(events, cfg, seen, now)
            for e in events:
                if e["count"] >= cfg["min_sightings"] and (e["hex"], e["code"]) not in before and now - e["last"] < 5:
                    log("NEW", e["code"], e.get("call"), e.get("reg"), e.get("type"), e.get("info"), e.get("near"))
            save_json(STATE_FILE, events)
            if len(routes) > 2000:
                routes = dict(sorted(routes.items(), key=lambda kv: kv[1]["at"])[-1000:])
            save_json(ROUTES_FILE, routes)
            payload(events, cfg, now, cfg["every_s"])
        except Exception as e:  # noqa: BLE001
            log("check failed:", e)
        time.sleep(max(10, cfg["every_s"] - (time.time() - start)))


if __name__ == "__main__":
    main()
