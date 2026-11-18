#!/bin/sh
# Installs the AI Usage updater as a launchd job that runs every 5 minutes (MacBook Pro only).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
[ -f "$DIR/clocks.txt" ] || cp "$DIR/clocks.example.txt" "$DIR/clocks.txt"
chmod +x "$DIR/refresh.sh"
PL="$HOME/Library/LaunchAgents/com.weatherclock.aiusage.plist"
cat > "$PL" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.weatherclock.aiusage</string>
  <key>ProgramArguments</key><array><string>/bin/sh</string><string>$DIR/refresh.sh</string></array>
  <key>StartInterval</key><integer>300</integer>
  <key>RunAtLoad</key><true/>
  <key>StandardOutPath</key><string>$DIR/ai-usage.log</string>
  <key>StandardErrorPath</key><string>$DIR/ai-usage.log</string>
</dict></plist>
PLIST
launchctl unload "$PL" 2>/dev/null || true
launchctl load "$PL"
echo "Installed. Log: $DIR/ai-usage.log"
