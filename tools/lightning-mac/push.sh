#!/bin/sh
# Sends the latest lightning summary (payload.txt, written by lightning.py) to every Weather Clock.
# Clocks: the addresses in clocks.txt (one per line) plus the other clocks they hear on the Wi-Fi.
# Runs every minute from launchd. Uses Apple's curl: macOS blocks background Python from the local network.
cd "$(dirname "$0")" || exit 1
c() { /usr/bin/curl --noproxy '*' -fsS --connect-timeout 5 --max-time 15 -H 'X-SmallTV: 1' "$@"; }

[ -f clocks.txt ] || { echo "clocks.txt is missing"; exit 1; }
seeds=$(grep -v '^#' clocks.txt | tr -d ' \r' | grep .)
all="$seeds"
first=""
for ip in $seeds; do
  peers=$(c "http://$ip/api/peers" 2>/dev/null) || continue
  [ -z "$first" ] && first=$ip
  all="$all $(printf '%s' "$peers" | grep -o '"ip":"[0-9.]*"' | cut -d'"' -f4)"
done
all=$(printf '%s\n' $all | sort -u)

# the clock's own city (Settings > Weather) decides where lightning is watched
if [ -n "$first" ]; then
  state=$(c "http://$first/api/state" 2>/dev/null)
  lat=$(printf '%s' "$state" | grep -o '"lat":-\{0,1\}[0-9.]*' | head -1 | cut -d: -f2)
  lon=$(printf '%s' "$state" | grep -o '"lon":-\{0,1\}[0-9.]*' | head -1 | cut -d: -f2)
  [ -n "$lat" ] && [ -n "$lon" ] && printf '{"lat": %s, "lon": %s}\n' "$lat" "$lon" > home.json.tmp && mv home.json.tmp home.json
fi

# only fresh data (lightning.py writes every 30 s): an old file means the feed is down, and the clocks then
# show "waiting" by themselves after 5 minutes
[ -f payload.txt ] || exit 0
[ -n "$(find payload.txt -mmin -3)" ] || { echo "$(date '+%F %T') payload is stale, not sent"; exit 0; }
for ip in $all; do
  c -X POST --data "@payload.txt" "http://$ip/api/lightning" >/dev/null 2>&1 \
    || echo "$(date '+%F %T') $ip did not answer"
done
