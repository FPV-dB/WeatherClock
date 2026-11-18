#!/bin/sh
PL="$HOME/Library/LaunchAgents/com.weatherclock.aiusage.plist"
launchctl unload "$PL" 2>/dev/null || true
rm -f "$PL"
echo "Removed the AI Usage job."
