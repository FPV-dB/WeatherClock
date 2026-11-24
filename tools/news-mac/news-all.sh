#!/bin/bash
# Sends the latest headlines to every Weather Clock listed in clocks.txt (one IP or name per line).
# news.py (from the radar helper) writes news-h0.txt..news-h7.txt and news-a0.txt..; this posts them to /api/news.
D="${NEWS_DIR:-$HOME/WeatherClockRadar}"; HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$D" || exit 1
"$D/.venv/bin/python" "$D/news.py" || exit 1
args=(); for k in 0 1 2 3 4 5 6 7; do [ -f "news-h$k.txt" ] && args+=(--data-urlencode "h$k@news-h$k.txt" --data-urlencode "a$k@news-a$k.txt"); done
[ ${#args[@]} -gt 0 ] || { echo "$(date) no headlines"; exit 1; }
grep -v -E '^\s*(#|$)' "$HERE/clocks.txt" | while read -r ip; do
  code=$(/usr/bin/curl -s -m10 -o /dev/null -w '%{http_code}' -X POST -H 'X-SmallTV: 1' "${args[@]}" "http://$ip/api/news")
  echo "$(date) news -> $ip: $code"
done
