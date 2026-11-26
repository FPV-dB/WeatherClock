#!/bin/sh
# Installs the Bad Squawks feed for the Weather Clock's Aircraft Bad Squawks face on this Mac (the always-on one).
# Copies the program to ~/WeatherClockSquawks and starts two background jobs:
#   com.weatherclock.squawks       squawks.py, always running (checks the ADS-B network every minute)
#   com.weatherclock.squawks-push  push.sh, every minute (sends the summary to the clocks)
set -e
SRC="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/WeatherClockSquawks"
AGENTS="$HOME/Library/LaunchAgents"
mkdir -p "$DEST" "$AGENTS"
cp "$SRC/squawks.py" "$SRC/push.sh" "$SRC/uninstall.sh" "$DEST/"
chmod +x "$DEST/squawks.py" "$DEST/push.sh" "$DEST/uninstall.sh"
[ -f "$DEST/config.json" ] || cp "$SRC/config.example.json" "$DEST/config.json"
if [ ! -f "$DEST/clocks.txt" ]; then
  if [ -n "$1" ]; then printf '%s\n' "$@" > "$DEST/clocks.txt"; else cp "$SRC/clocks.example.txt" "$DEST/clocks.txt"; fi
fi

cat > "$AGENTS/com.weatherclock.squawks.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.weatherclock.squawks</string>
  <key>ProgramArguments</key><array><string>/usr/bin/python3</string><string>$DEST/squawks.py</string></array>
  <key>WorkingDirectory</key><string>$DEST</string>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>ThrottleInterval</key><integer>30</integer>
  <key>StandardOutPath</key><string>$DEST/squawks.log</string>
  <key>StandardErrorPath</key><string>$DEST/squawks.log</string>
</dict></plist>
PLIST

cat > "$AGENTS/com.weatherclock.squawks-push.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.weatherclock.squawks-push</string>
  <key>ProgramArguments</key><array><string>/bin/sh</string><string>$DEST/push.sh</string></array>
  <key>WorkingDirectory</key><string>$DEST</string>
  <key>RunAtLoad</key><true/>
  <key>StartInterval</key><integer>60</integer>
  <key>StandardOutPath</key><string>$DEST/push.log</string>
  <key>StandardErrorPath</key><string>$DEST/push.log</string>
</dict></plist>
PLIST

for job in squawks squawks-push; do
  launchctl bootout "gui/$(id -u)/com.weatherclock.$job" 2>/dev/null || true
  launchctl bootstrap "gui/$(id -u)" "$AGENTS/com.weatherclock.$job.plist"
done
echo "Installed in $DEST. Clocks: $(grep -v '^#' "$DEST/clocks.txt" | tr '\n' ' ')"
echo "Logs: $DEST/squawks.log and $DEST/push.log"
