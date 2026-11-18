#!/usr/bin/env python3
"""Work out the Claude and OpenAI Codex usage limits for the Weather Clock "AI Usage" face.

Prints one line of form data for POST /api/aiusage (c5 c7 cr5 cr7 for Claude, x5 x7 xr5 xr7 for Codex:
% left of the 5-hour and 7-day windows, and when each resets in seconds since 1970 UTC).
This script only talks to the internet; refresh.sh sends the line to the clocks with Apple's curl
(macOS blocks Python background jobs from the local network).

Where the numbers come from, on this Mac:
  Claude  the Claude Code sign-in (macOS Keychain item "Claude Code-credentials"), asking the same usage
          endpoint Claude Code's /usage screen uses.
  Codex   the Codex sign-in (~/.codex/auth.json), asking the same endpoint Codex's /status uses; if that
          fails, the limits Codex last wrote in its own session logs (~/.codex/sessions).
Neither endpoint is a published API, so either can change without notice. Sign-ins are only read.
"""

import glob
import json
import os
import subprocess
import sys
import time
from datetime import datetime

HOME = os.path.expanduser("~")
LOG = []


def note(msg):
    LOG.append(msg)


def curl_json(url, headers):
    args = ["/usr/bin/curl", "-fsS", "--connect-timeout", "10", "--max-time", "30", url]
    for k, v in headers.items():
        args += ["-H", f"{k}: {v}"]
    return json.loads(subprocess.check_output(args, text=True, stderr=subprocess.PIPE))


def epoch(v):
    """ISO-8601 text or a number -> seconds since 1970 UTC (0 if unknown)."""
    if v is None:
        return 0
    if isinstance(v, (int, float)):
        return int(v)
    try:
        return int(datetime.fromisoformat(str(v).replace("Z", "+00:00")).timestamp())
    except ValueError:
        return 0


def left(used):
    return max(0, min(100, round(100 - float(used))))


def after_reset(pct_left, reset):
    """A window whose reset time has passed is full again."""
    return 100 if reset and reset <= time.time() else pct_left


# ---------- Claude ----------
def claude_token():
    try:
        raw = subprocess.check_output(
            ["/usr/bin/security", "find-generic-password", "-s", "Claude Code-credentials", "-w"],
            text=True, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        path = os.path.join(HOME, ".claude", ".credentials.json")
        if not os.path.exists(path):
            return None
        raw = open(path).read()
    return json.loads(raw).get("claudeAiOauth", {}).get("accessToken")


def claude():
    tok = claude_token()
    if not tok:
        note("Claude: not signed in to Claude Code on this Mac (run `claude` and /login)")
        return None
    try:
        u = curl_json("https://api.anthropic.com/api/oauth/usage",
                      {"Authorization": f"Bearer {tok}", "anthropic-beta": "oauth-2025-04-20"})
    except subprocess.CalledProcessError as e:
        note(f"Claude: usage request failed ({e.stderr.strip()[:120]}); open Claude Code once to renew the sign-in")
        return None
    out = {}
    for key, name in (("five_hour", "5"), ("seven_day", "7")):
        w = u.get(key) or {}
        r = epoch(w.get("resets_at"))
        out[name] = after_reset(left(w.get("utilization") or 0), r)
        out["r" + name] = r
    return out


# ---------- Codex ----------
def codex_live():
    path = os.path.join(os.environ.get("CODEX_HOME", os.path.join(HOME, ".codex")), "auth.json")
    if not os.path.exists(path):
        note("Codex: not signed in on this Mac (run `codex login`)")
        return None
    t = json.load(open(path)).get("tokens") or {}
    if not t.get("access_token"):
        note("Codex: signed in with an API key, which has no plan limits")
        return None
    try:
        u = curl_json("https://chatgpt.com/backend-api/wham/usage",
                      {"Authorization": f"Bearer {t['access_token']}",
                       "ChatGPT-Account-Id": t.get("account_id", ""), "User-Agent": "codex-cli"})
    except subprocess.CalledProcessError as e:
        note(f"Codex: usage request failed ({e.stderr.strip()[:120]}); trying the session logs")
        return None
    rl = u.get("rate_limit") or {}
    out = {}
    for key, name in (("primary_window", "5"), ("secondary_window", "7")):
        w = rl.get(key) or {}
        r = epoch(w.get("reset_at")) or (int(time.time() + w["reset_after_seconds"]) if "reset_after_seconds" in w else 0)
        out[name] = after_reset(left(w.get("used_percent") or 0), r)
        out["r" + name] = r
    return out


def codex_logs():
    """The newest rate_limits Codex wrote to its session logs (only as fresh as the last Codex use)."""
    root = os.path.join(os.environ.get("CODEX_HOME", os.path.join(HOME, ".codex")), "sessions")
    files = sorted(glob.glob(os.path.join(root, "**", "*.jsonl"), recursive=True), key=os.path.getmtime)
    for f in reversed(files[-20:]):
        last = None
        with open(f, errors="replace") as fh:
            for line in fh:
                if '"rate_limits"' in line:
                    last = line
        if not last:
            continue
        def find(o):
            if isinstance(o, dict):
                if isinstance(o.get("rate_limits"), dict):
                    return o["rate_limits"]
                for v in o.values():
                    r = find(v)
                    if r:
                        return r
            return None
        rl = find(json.loads(last)) or {}
        out = {}
        for key, name in (("primary", "5"), ("secondary", "7")):
            w = rl.get(key) or {}
            r = epoch(w.get("resets_at"))
            out[name] = after_reset(left(w.get("used_percent") or 0), r)
            out["r" + name] = r
        note(f"Codex: from the session log {os.path.basename(f)}")
        return out
    note("Codex: no limits in the session logs yet")
    return None


def main():
    form = []
    for prefix, data in (("c", claude()), ("x", codex_live() or codex_logs())):
        if data:
            form += [f"{prefix}5={data['5']}", f"{prefix}7={data['7']}",
                     f"{prefix}r5={data['r5']}", f"{prefix}r7={data['r7']}"]
    for line in LOG:
        print(line, file=sys.stderr)
    if not form:
        sys.exit(1)
    print("&".join(form))


if __name__ == "__main__":
    main()
