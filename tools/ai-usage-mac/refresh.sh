#!/bin/sh
# AI Usage face: works out the limits (ai_usage.py, internet only) and sends them to every clock in clocks.txt
# with Apple's curl. Run every 5 minutes by launchd (install.sh). Log: ai-usage.log
cd "$(dirname "$0")" || exit 1
echo "--- $(date '+%Y-%m-%d %H:%M:%S')"
form=$(/usr/bin/python3 ai_usage.py) || { echo "no usage data"; exit 1; }
echo "$form"
grep -v '^ *#' clocks.txt | while read -r host _; do
  [ -n "$host" ] || continue
  if /usr/bin/curl --noproxy '*' -fsS --connect-timeout 5 --max-time 15 -H 'X-SmallTV: 1' \
       -X POST --data "$form" "http://$host/api/aiusage" >/dev/null; then
    echo "$host ok"
  else
    echo "$host failed"
  fi
done
