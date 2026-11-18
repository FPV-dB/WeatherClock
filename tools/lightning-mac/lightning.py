#!/usr/bin/env python3
"""Live lightning near the Weather Clock, for its Lightning face.

Follows the Blitzortung.org community lightning network over a websocket and keeps the strikes that fall
within the radius around the clock. Every 30 seconds it writes payload.txt, which push.sh sends to the
clocks. Internet only: this program never talks to the local network (macOS blocks background Python
from it), push.sh does that with curl.

Standard library only, works with Apple's /usr/bin/python3 (3.9).
"""

import base64
import json
import math
import os
import pathlib
import random
import socket
import ssl
import struct
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
CONFIG_FILE = HERE / "config.json"
HOME_FILE = HERE / "home.json"        # written by push.sh from the clock's own settings
PAYLOAD_FILE = HERE / "payload.txt"
STATE_FILE = HERE / "state.json"      # time of the last strike, kept across restarts

WRITE_EVERY = 30
KEEP = 3600                           # seconds of strikes kept in memory
SERVERS = ["ws1", "ws2", "ws3", "ws7", "ws8"]


def log(*args):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), *args, flush=True)


def load_config():
    cfg = {"radius_km": 100}
    if CONFIG_FILE.exists():
        cfg.update(json.loads(CONFIG_FILE.read_text()))
    return cfg


def home(cfg):
    """The clock's place: from the clock (home.json), else from config.json."""
    try:
        h = json.loads(HOME_FILE.read_text())
        if abs(h["lat"]) > 0.01 or abs(h["lon"]) > 0.01:
            return float(h["lat"]), float(h["lon"])
    except Exception:
        pass
    if "lat" in cfg and "lon" in cfg:
        return float(cfg["lat"]), float(cfg["lon"])
    return None


# ---------- geometry ----------
def distance_km(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 6371.0 * 2 * math.asin(min(1.0, math.sqrt(a)))


def bearing(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    y = math.sin(dl) * math.cos(p2)
    x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(y, x)) + 360) % 360


def centroid(strikes):
    return (sum(s[1] for s in strikes) / len(strikes), sum(s[2] for s in strikes) / len(strikes))


# ---------- Blitzortung messages ----------
def lzw_decode(data):
    """The live map's messages are LZW-packed into a string of characters."""
    if not data:
        return ""
    table = {}
    chars = list(data)
    c = chars[0]
    prev = c
    out = [c]
    code = 256
    for ch in chars[1:]:
        n = ord(ch)
        if n < 256:
            entry = ch
        elif n in table:
            entry = table[n]
        else:
            entry = prev + c
        out.append(entry)
        c = entry[0]
        table[code] = prev + c
        code += 1
        prev = entry
    return "".join(out)


def parse_strike(message):
    """(unix seconds, lat, lon) or None."""
    try:
        d = json.loads(message)
    except ValueError:
        try:
            d = json.loads(lzw_decode(message))
        except ValueError:
            return None
    if not isinstance(d, dict) or "lat" not in d or "lon" not in d:
        return None
    t = d.get("time")
    t = t / 1e9 if isinstance(t, (int, float)) and t > 1e12 else time.time()
    return (t, float(d["lat"]), float(d["lon"]))


# ---------- a small websocket client (text frames only) ----------
class WebSocket:
    def __init__(self, host, timeout=60):
        ctx = ssl.create_default_context()
        if os.path.exists("/etc/ssl/cert.pem"):  # macOS system roots, for Apple's Python
            ctx.load_verify_locations("/etc/ssl/cert.pem")
        raw = socket.create_connection((host, 443), timeout=20)
        self.sock = ctx.wrap_socket(raw, server_hostname=host)
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            "GET / HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Origin: https://map.blitzortung.org\r\n"
            "User-Agent: WeatherClock-Lightning/1.0\r\n\r\n"
        )
        self.sock.sendall(request.encode())
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = self.sock.recv(1024)
            if not chunk:
                raise ConnectionError("closed during handshake")
            head += chunk
            if len(head) > 16384:
                raise ConnectionError("handshake too long")
        head, self.buf = head.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise ConnectionError(head.split(b"\r\n", 1)[0].decode(errors="replace"))
        self.sock.settimeout(timeout)

    def _read(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("connection closed")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def send(self, opcode, payload):
        mask = os.urandom(4)
        n = len(payload)
        if n < 126:
            head = struct.pack("!BB", 0x80 | opcode, 0x80 | n)
        elif n < 65536:
            head = struct.pack("!BBH", 0x80 | opcode, 0xFE, n)
        else:
            head = struct.pack("!BBQ", 0x80 | opcode, 0xFF, n)
        body = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        self.sock.sendall(head + mask + body)

    def send_text(self, text):
        self.send(0x1, text.encode())

    def recv_text(self):
        parts = []
        while True:
            b0, b1 = self._read(2)
            opcode, fin = b0 & 0x0F, b0 & 0x80
            n = b1 & 0x7F
            if n == 126:
                n = struct.unpack("!H", self._read(2))[0]
            elif n == 127:
                n = struct.unpack("!Q", self._read(8))[0]
            if n > 8 * 1024 * 1024:
                raise ConnectionError("frame too large")
            mask = self._read(4) if b1 & 0x80 else None
            data = self._read(n)
            if mask:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            if opcode == 0x8:
                raise ConnectionError("server closed the connection")
            if opcode == 0x9:
                self.send(0xA, data)
                continue
            if opcode == 0xA:
                continue
            parts.append(data)
            if fin:
                return b"".join(parts).decode("utf-8", errors="replace")

    def close(self):
        try:
            self.send(0x8, b"")
        except Exception:
            pass
        self.sock.close()


# ---------- what the clock gets ----------
def summary(strikes, place, radius, last_strike, now):
    lat0, lon0 = place
    recent = [s for s in strikes if now - s[0] <= 600]
    half = [s for s in strikes if now - s[0] <= 1800]
    near_set = recent or half
    out = {"r": radius, "n": len(recent), "near": -1, "from": -1, "mv": -1,
           "ago": int(now - last_strike) if last_strike else -1}
    if near_set:
        out["near"] = int(round(min(s[3] for s in near_set)))
        c = centroid(near_set)
        out["from"] = int(round(bearing(lat0, lon0, c[0], c[1]))) % 360
        older = [s for s in strikes if 600 < now - s[0] <= 1200]
        if len(recent) >= 5 and len(older) >= 5:
            c_old = centroid(older)
            if distance_km(c_old[0], c_old[1], c[0], c[1]) >= 2:  # moved at least 2 km in 10 minutes
                out["mv"] = int(round(bearing(c_old[0], c_old[1], c[0], c[1]))) % 360
    return out


def write_payload(values):
    text = "&".join(f"{k}={v}" for k, v in values.items())
    tmp = PAYLOAD_FILE.with_suffix(".tmp")
    tmp.write_text(text + "\n")
    os.replace(tmp, PAYLOAD_FILE)


def main():
    cfg = load_config()
    radius = int(cfg.get("radius_km", 100))
    strikes = []  # (time, lat, lon, km)
    last_strike = 0.0
    try:
        last_strike = float(json.loads(STATE_FILE.read_text()).get("last_strike", 0))
    except Exception:
        pass
    backoff = 5
    last_write = 0.0
    while True:
        place = home(cfg)
        if not place:
            log("waiting for the clock's place (home.json from push.sh, or lat/lon in config.json)")
            time.sleep(30)
            continue
        host = random.choice(SERVERS) + ".blitzortung.org"
        ws = None
        try:
            ws = WebSocket(host)
            ws.send_text('{"a":111}')
            log("connected to", host)
            backoff = 5
            while True:
                strike = parse_strike(ws.recv_text())
                now = time.time()
                if strike:
                    km = distance_km(place[0], place[1], strike[1], strike[2])
                    if km <= radius:
                        strikes.append((strike[0], strike[1], strike[2], km))
                        if strike[0] > last_strike:
                            last_strike = strike[0]
                            STATE_FILE.write_text(json.dumps({"last_strike": last_strike}))
                if now - last_write >= WRITE_EVERY:
                    last_write = now
                    strikes = [s for s in strikes if now - s[0] <= KEEP]
                    new_place = home(cfg)
                    if new_place and new_place != place:  # the clock moved city: start over
                        place, strikes = new_place, []
                    write_payload(summary(strikes, place, radius, last_strike, now))
        except Exception as error:
            log("feed error:", error)
        finally:
            if ws:
                ws.close()
        time.sleep(backoff)
        backoff = min(backoff * 2, 300)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--once":
        print(summary([], (0, 0), 100, 0, time.time()))
    else:
        main()
