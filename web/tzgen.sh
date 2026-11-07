#!/bin/sh
# Builds web/tz.json: time zone name (IANA, what Open-Meteo returns) -> POSIX TZ rule (what the clock uses).
# The rule is the last line of each tzdata file (TZif v2+ format). Run on Linux/WSL with an up-to-date
# tzdata package; the panel downloads the result from the site (panel/tz-<code>.json, with SRI) when the city is chosen.
cd "$(dirname "$0")"
Z=/usr/share/zoneinfo
V=$(cat "$Z/tzdata.zi" 2>/dev/null | head -1 | sed 's/^# version //')
{
  printf '{"_versao":"%s"' "$V"
  for d in Africa America Antarctica Arctic Asia Atlantic Australia Europe Indian Pacific Etc; do
    find "$Z/$d" -type f | sort | while read -r f; do
      p=$(tail -n1 "$f")
      case "$p" in ""|TZif*) continue ;; esac
      printf ',"%s":"%s"' "${f#$Z/}" "$p"
    done
  done
  printf '}\n'
} > tz.json
echo "tz.json: $(grep -o '":"' tz.json | wc -l) entradas (inclui _versao), tzdata $V"
