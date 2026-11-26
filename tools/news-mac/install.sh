#!/bin/bash
# Installs a launchd job that runs news-all.sh every 10 minutes (macOS). Run it on the computer that keeps helpers running.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
[ -f "$HERE/clocks.txt" ] || { cp "$HERE/clocks.example.txt" "$HERE/clocks.txt"; echo "Edit $HERE/clocks.txt with your clocks' addresses, then run install.sh again."; exit 1; }
chmod +x "$HERE/news-all.sh"
P="$HOME/Library/LaunchAgents/com.weatherclock.news.plist"
cat > "$P" <<PL
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict><key>Label</key><string>com.weatherclock.news</string>
<key>ProgramArguments</key><array><string>/bin/bash</string><string>$HERE/news-all.sh</string></array>
<key>StartInterval</key><integer>600</integer><key>RunAtLoad</key><true/>
<key>StandardOutPath</key><string>$HERE/news-all.log</string><key>StandardErrorPath</key><string>$HERE/news-all.log</string>
</dict></plist>
PL
launchctl unload "$P" 2>/dev/null || true
launchctl load "$P"
echo "Installed. Log: $HERE/news-all.log"
