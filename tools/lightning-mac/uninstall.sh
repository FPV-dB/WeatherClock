#!/bin/sh
# Stops and removes the lightning feed jobs (the ~/WeatherClockLightning folder is kept).
for job in lightning lightning-push; do
  launchctl bootout "gui/$(id -u)/com.weatherclock.$job" 2>/dev/null || true
  rm -f "$HOME/Library/LaunchAgents/com.weatherclock.$job.plist"
done
echo "Lightning feed stopped."
