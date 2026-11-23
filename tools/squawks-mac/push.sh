#!/bin/sh
# Sends the latest Bad Squawks list (payload.txt, written by squawks.py) to every Weather Clock.
# Clocks: the addresses in clocks.txt (one per line) plus the other clocks they hear on the Wi-Fi.
# Runs every minute from launchd. Uses Apple's curl: macOS blocks background Python from the local network.
cd "$(dirname "$0")" || exit 1
c() { /usr/bin/curl --noproxy '*' -fsS --connect-timeout 5 --max-time 15 -H 'X-SmallTV: 1' "$@"; }

[ -f clocks.txt ] || { echo "clocks.txt is missing"; exit 1; }
seeds=$(grep -v '^#' clocks.txt | tr -d ' \r' | grep .)
all="$seeds"
for ip in $seeds; do
  peers=$(c "http://$ip/api/peers" 2>/dev/null) || continue
  all="$all $(printf '%s' "$peers" | grep -o '"ip":"[0-9.]*"' | cut -d'"' -f4)"
done
all=$(printf '%s\n' $all | sort -u)

# only fresh data (squawks.py writes every minute): an old file means the feed is down
[ -s payload.txt ] || exit 0
[ -n "$(find payload.txt -mmin -5)" ] || { echo "$(date '+%F %T') payload is stale, not sent"; exit 0; }
for ip in $all; do
  c -X POST --data "@payload.txt" "http://$ip/api/squawks" >/dev/null 2>&1 \
    || echo "$(date '+%F %T') $ip did not answer"
done
