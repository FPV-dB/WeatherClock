#!/usr/bin/env python3
"""Feed the Weather Clock Energy War face (firmware 2.0.15 or newer)."""

import base64
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.parse
from datetime import datetime
import time

HERE = pathlib.Path(__file__).resolve().parent
CONFIG = json.loads((HERE / "config.json").read_text())
HOST = CONFIG["clock_ip"]
EXPECTED_MAC = CONFIG["expected_mac"].lower()
GH = pathlib.Path(CONFIG.get("gh") or shutil.which("gh") or pathlib.Path.home() / ".local/bin/gh")
SOURCE_CACHE = HERE / "incidents.ts"
FLIGHT_STATE = pathlib.Path(CONFIG["flight_state"]).expanduser()
STATS_URL = CONFIG.get("stats_url", "https://energywar.net/stats.json?limit=5")
SOURCE_REPOSITORY = CONFIG.get("source_repository", "FPV-dB/WorldWarE")


def curl(path, *args):
    return subprocess.check_output(
        [
            "/usr/bin/curl", "--noproxy", "*", "-fsS",
            "--connect-timeout", "5", "--max-time", "30",
            "-H", "X-SmallTV: 1", *args, f"http://{HOST}{path}",
        ],
        text=True,
    )


def post(path, values):
    return curl(path, "-X", "POST", "--data", urllib.parse.urlencode(values))


def verify_target():
    state = json.loads(curl("/api/state"))
    arp = subprocess.check_output(["/usr/sbin/arp", "-n", HOST], text=True)
    match = re.search(r" at ([0-9a-f:]+) ", arp.lower())
    actual = ":".join(part.zfill(2) for part in match.group(1).split(":")) if match else ""
    if actual != EXPECTED_MAC:
        raise RuntimeError(f"target MAC mismatch: expected {EXPECTED_MAC}, got {actual or 'unknown'}")
    ver = str(state.get("ver") or "")
    parts = tuple(int(x) for x in re.findall(r"\d+", ver)[:3])
    if len(parts) < 3 or parts < (2, 0, 15):
        raise RuntimeError(f"target firmware is {ver!r}, expected 2.0.15 or newer")


def refresh_source():
    try:
        raw = subprocess.check_output(
            [
                str(GH), "api", "-X", "GET",
                f"repos/{SOURCE_REPOSITORY}/contents/app/incidents.ts",
                "--jq", ".content",
            ],
            text=True,
            timeout=45,
        )
        source = base64.b64decode("".join(raw.split())).decode("utf-8")
        temporary = SOURCE_CACHE.with_suffix(".tmp")
        temporary.write_text(source)
        os.replace(temporary, SOURCE_CACHE)
        return source
    except Exception as error:
        if SOURCE_CACHE.exists():
            print(f"source refresh failed; using cache: {error}", flush=True)
            return SOURCE_CACHE.read_text()
        raise


def object_blocks(source):
    start = source.index("export const incidents")
    start = source.index("=", start)
    start = source.index("[", start)
    blocks = []
    depth = 0
    quote = None
    escaped = False
    block_start = None
    for index in range(start + 1, len(source)):
        char = source[index]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in ('"', "'", "`"):
            quote = char
        elif char == "{":
            if depth == 0:
                block_start = index
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0 and block_start is not None:
                blocks.append(source[block_start:index + 1])
                block_start = None
        elif char == "]" and depth == 0:
            break
    return blocks


def string_field(block, name):
    match = re.search(rf"\b{re.escape(name)}\s*:\s*(\"(?:\\.|[^\"\\])*\")", block, re.S)
    return json.loads(match.group(1)) if match else None


def parsed_date(value):
    match = re.search(r"\b(\d{1,2})\s+([A-Za-z]+)\s+(\d{4})\b", value or "")
    if not match:
        return datetime.min
    try:
        return datetime.strptime(" ".join(match.groups()), "%d %B %Y")
    except ValueError:
        return datetime.min


def energy_payload(source):
    rows = []
    for block in object_blocks(source):
        row = {key: string_field(block, key) for key in (
            "name", "date", "status", "infrastructure", "summary", "incidentType"
        )}
        if all(row.get(key) for key in ("name", "date", "status", "infrastructure")):
            rows.append(row)
    rows.sort(key=lambda row: parsed_date(row["date"]), reverse=True)
    payload = {"total": len(rows)}
    status_letter = {
        "documented": "D", "reported": "R", "unconfirmed": "U", "investigating": "I"
    }
    for index, row in enumerate(rows[:5]):
        date = parsed_date(row["date"])
        payload.update({
            f"p{index}": row["name"][:45],
            f"w{index}": (row.get("summary") or row["name"])[:91],
            f"k{index}": row["infrastructure"][:23],
            f"d{index}": date.strftime("%d %b").upper() if date != datetime.min else row["date"][:7],
            f"s{index}": status_letter.get(row["status"], "-"),
        })
    return payload, rows[:5]


def flight_payload():
    if not FLIGHT_STATE.exists():
        raise RuntimeError(f"airport feed state is missing: {FLIGHT_STATE}")
    age = time.time() - FLIGHT_STATE.stat().st_mtime
    if age > 30 * 60:
        raise RuntimeError(f"airport feed state is stale ({int(age // 60)} minutes old): {FLIGHT_STATE}")
    state = json.loads(FLIGHT_STATE.read_text())
    today = datetime.now().astimezone().date().isoformat()
    if state.get("date") != today:
        raise RuntimeError(f"airport feed date is {state.get('date')!r}, expected {today!r}")
    counts = state.get("counts", {})
    payload = {
        "date": state["date"], "city": state["city"],
        "off": sum(value["off"] for value in counts.values()),
        "on": sum(value["on"] for value in counts.values()),
        "partial": int(state.get("partial", False)),
    }
    ordered = sorted(counts.items(), key=lambda item: -(item[1]["off"] + item[1]["on"]))[:32]
    for index, (code, value) in enumerate(ordered):
        payload.update({f"c{index}": code, f"o{index}": value["off"], f"l{index}": value["on"]})
    return payload


def live_payload():
    """The live site's own numbers: https://energywar.net/stats.json (total = homepage headline)."""
    import urllib.request
    req = urllib.request.Request(STATS_URL, headers={"User-Agent": "WeatherClock/1.0"})
    with urllib.request.urlopen(req, timeout=30) as response:
        stats = json.load(response)
    status_letter = {"documented": "D", "reported": "R", "unconfirmed": "U", "investigating": "I"}
    payload = {"total": int(stats["total"])}
    rows = stats.get("latest") or []
    for index, row in enumerate(rows[:5]):
        name = row.get("name") or ""
        place = name.split(" \u2014 ")[0]
        what = name.split(" \u2014 ", 1)[1] if " \u2014 " in name else name
        try:
            day = datetime.strptime(row.get("eventStartDate") or "", "%Y-%m-%d").strftime("%d %b").upper()
        except ValueError:
            day = (row.get("date") or "")[:6].upper()
        payload.update({
            f"p{index}": place[:45],
            f"w{index}": what[:91],
            f"k{index}": (row.get("infrastructure") or "")[:23],
            f"d{index}": day,
            f"s{index}": status_letter.get(row.get("status") or "", "-"),
        })
    return payload, rows[:5]


def main():
    verify_target()
    try:
        energy, newest = live_payload()
    except Exception as error:
        print(f"stats.json failed ({error}); using the GitHub incident list", flush=True)
        energy, newest = energy_payload(refresh_source())
    if not newest:
        raise RuntimeError("no EnergyWar incidents parsed; no writes performed")
    if "--dry-run" in sys.argv:
        print(json.dumps({"energy": energy, "newest": newest, "flight": flight_payload()}, indent=2))
        return
    post("/api/energy", energy)
    post("/api/flightstats", flight_payload())
    print(
        f"ok: energy {energy['total']} records, newest {energy['p0']}; "
        f"airport data sent to {HOST}",
        flush=True,
    )


if __name__ == "__main__":
    main()
